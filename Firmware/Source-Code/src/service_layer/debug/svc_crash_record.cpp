/**
 * @name Hornet / WPT Charger
 * @file svc_crash_record.cpp
 * @brief Post-mortem fault capture in no-init RAM, reported on the next boot
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "svc_crash_record.h"

#include "svc_debug_config.h"

#include "eda_active_object.h"
#include "eda_manager_log_config.h"

#include "app_error.h"
#include "app_util_platform.h"
#include "nrf.h"
#include "nrf_strerror.h"

#ifdef SOFTDEVICE_PRESENT
#include "nrf_sdm.h"
#endif

#include <timers.h>

#include <cstring>

/// Ends of the main stack, placed by flash_placement.xml (.stack carries
/// __StackLimit / __StackTop). Only their addresses are meaningful.
extern "C" uint32_t __StackLimit;
extern "C" uint32_t __StackTop;

/// Tail of HardFault_Handler, which has to be reached from naked assembly.
extern "C" void crash_record_hard_fault(uint32_t *p_frame);

namespace
{
    /// Written into the record and the counters once each is populated. Both live
    /// in RAM that nothing initializes, so without a magic word there is no way to
    /// tell a real record from whatever the last power-up left behind.
    constexpr uint32_t RECORD_MAGIC = 0xC7A54AEDu;
    constexpr uint32_t COUNTER_MAGIC = 0xC7A5C047u;

    constexpr uint8_t FILE_LEN = 32;
    constexpr uint8_t TASK_LEN = 16;

    /// Words left alone below the stack pointer when painting, so the paint loop
    /// cannot overwrite the frame it is running in.
    constexpr uint32_t PAINT_MARGIN_WORDS = 64;
    constexpr uint32_t PAINT_WORD = 0xA5A5A5A5u;

    constexpr uint32_t RAM_START = 0x20000000u;
    constexpr uint32_t RAM_END = 0x20040000u;   // nRF52840: 256 kB
    constexpr uint32_t FLASH_END = 0x00100000u; // nRF52840: 1 MB

    struct CrashData_t
    {
        uint32_t magic;
        uint32_t type;
        uint32_t pc;
        uint32_t lr;
        uint32_t psr;
        uint32_t err_code;
        uint32_t line;
        uint32_t cfsr;
        uint32_t hfsr;
        uint32_t mmfar;
        uint32_t bfar;
        uint32_t heartbeat;
        char file[FILE_LEN];
        char task[TASK_LEN];
    };

    struct Counters_t
    {
        uint32_t magic;
        uint32_t boots;
        uint32_t crashes;
        uint32_t heartbeat;
    };

    /// .non_init is declared load="No" in flash_placement.xml and the SES startup
    /// only copies the .nrf_sections group, so neither object is touched on a soft
    /// reset. A power-on reset leaves them as undefined RAM, which is what the
    /// magic words above are for.
    __attribute__((section(".non_init"))) CrashData_t m_record;
    __attribute__((section(".non_init"))) Counters_t m_counters;

    /// True when the whole range can be read without faulting. Anything reached
    /// from a fault handler - an info pointer, an exception frame - has to pass
    /// this first, because the reason we are here may be that it is corrupt.
    bool IsReadable(uint32_t address, uint32_t length)
    {
        const uint32_t end = address + length;

        if (end < address)
        {
            return false;
        }

        if ((address >= RAM_START) && (end <= RAM_END))
        {
            return true;
        }

        return (end <= FLASH_END);
    }

    /// Strips the directory part of __FILE__, which is an absolute build path and
    /// would otherwise fill the record.
    char const *BaseName(uint8_t const *p_path)
    {
        if ((p_path == nullptr) || !IsReadable(reinterpret_cast<uint32_t>(p_path), 1u))
        {
            return nullptr;
        }

        char const *const p_text = reinterpret_cast<char const *>(p_path);
        char const *p_base = p_text;

        for (uint32_t i = 0; (i < 256u) && (p_text[i] != '\0'); i++)
        {
            if ((p_text[i] == '/') || (p_text[i] == '\\'))
            {
                p_base = &p_text[i + 1u];
            }
        }

        return p_base;
    }

    void CopyName(char *p_dst, uint8_t dst_len, char const *p_src)
    {
        if ((p_dst == nullptr) || (dst_len == 0u))
        {
            return;
        }

        if (p_src == nullptr)
        {
            p_dst[0] = '\0';
            return;
        }

        uint8_t i = 0;

        while (((i + 1u) < dst_len) && (p_src[i] != '\0'))
        {
            p_dst[i] = p_src[i];
            i++;
        }

        p_dst[i] = '\0';
    }

    char const *CurrentTaskName()
    {
        if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED)
        {
            return "boot";
        }

        TaskHandle_t const handle = xTaskGetCurrentTaskHandle();

        return (handle != nullptr) ? pcTaskGetName(handle) : nullptr;
    }

    /// Opens a record, unless this boot already has one. Keeping the first fault
    /// matters: if logging or the reset path faults again, the original cause is
    /// what we want to read back.
    bool StartRecord(uint32_t type)
    {
        if (m_record.magic == RECORD_MAGIC)
        {
            return false;
        }

        memset(&m_record, 0, sizeof(m_record));
        m_record.type = type;
        m_record.heartbeat = (m_counters.magic == COUNTER_MAGIC) ? m_counters.heartbeat : 0u;
        CopyName(m_record.task, TASK_LEN, CurrentTaskName());

        return true;
    }

    void CommitRecord()
    {
        if (m_counters.magic == COUNTER_MAGIC)
        {
            m_counters.crashes++;
        }

        m_record.magic = RECORD_MAGIC;
    }

    char const *TypeText(uint32_t type)
    {
        switch (type)
        {
        case svc::CrashRecord::TYPE_SDK_ERROR:
            return "SDK error";
        case svc::CrashRecord::TYPE_SDK_ASSERT:
            return "assertion";
        case svc::CrashRecord::TYPE_SD_ASSERT:
            return "SoftDevice assertion";
        case svc::CrashRecord::TYPE_APP_MEMACC:
            return "invalid memory access";
        case svc::CrashRecord::TYPE_HARD_FAULT:
            return "HARD FAULT";
        case svc::CrashRecord::TYPE_STACK_OVERFLOW:
            return "stack overflow";
        default:
            return "unknown fault";
        }
    }

    uint32_t FaultType(uint32_t id)
    {
        switch (id)
        {
        case NRF_FAULT_ID_SDK_ERROR:
            return svc::CrashRecord::TYPE_SDK_ERROR;
        case NRF_FAULT_ID_SDK_ASSERT:
            return svc::CrashRecord::TYPE_SDK_ASSERT;
#ifdef SOFTDEVICE_PRESENT
        case NRF_FAULT_ID_SD_ASSERT:
            return svc::CrashRecord::TYPE_SD_ASSERT;
        case NRF_FAULT_ID_APP_MEMACC:
            return svc::CrashRecord::TYPE_APP_MEMACC;
#endif
        default:
            return svc::CrashRecord::TYPE_UNKNOWN_FAULT;
        }
    }

    /// Names the first reset cause that is set. RESETREAS with no bits set means
    /// the supply was interrupted - a power-on or brown-out reset - which is the
    /// one case that also wipes the record.
    char const *ResetReasonText(uint32_t reasons)
    {
        if (reasons == 0u)
        {
            return "power-on or brown-out";
        }

        if ((reasons & POWER_RESETREAS_RESETPIN_Msk) != 0u)
        {
            return "pin reset";
        }

        if ((reasons & POWER_RESETREAS_DOG_Msk) != 0u)
        {
            return "watchdog";
        }

        if ((reasons & POWER_RESETREAS_SREQ_Msk) != 0u)
        {
            return "software reset";
        }

        if ((reasons & POWER_RESETREAS_LOCKUP_Msk) != 0u)
        {
            return "CPU lockup";
        }

        if ((reasons & POWER_RESETREAS_OFF_Msk) != 0u)
        {
            return "wake from system OFF";
        }

        if ((reasons & POWER_RESETREAS_DIF_Msk) != 0u)
        {
            return "debug interface";
        }

        return "other";
    }

    void PaintMainStack()
    {
        uint32_t *const p_bottom = &__StackLimit;
        uint32_t *const p_sp = reinterpret_cast<uint32_t *>(__get_MSP());

        if (p_sp <= p_bottom)
        {
            return;
        }

        const uint32_t span_words = static_cast<uint32_t>(p_sp - p_bottom);

        if (span_words <= PAINT_MARGIN_WORDS)
        {
            return;
        }

        uint32_t *const p_top = p_sp - PAINT_MARGIN_WORDS;

        for (uint32_t *p = p_bottom; p < p_top; p++)
        {
            *p = PAINT_WORD;
        }
    }

    uint32_t MainStackFreeBytes()
    {
        uint32_t const *p = &__StackLimit;
        uint32_t const *const p_end = &__StackTop;
        uint32_t words = 0;

        while ((p < p_end) && (*p == PAINT_WORD))
        {
            words++;
            p++;
        }

        return words * sizeof(uint32_t);
    }

    /// Shared tail of every fault path: one best-effort log line, then reset.
    ///
    /// The record is already in RAM that survives the reset, so nothing here is
    /// load-bearing - if the logger is the thing that broke, the next boot still
    /// reports the fault.
    void FinishFault()
    {
        LOG_ERROR("CRASH: %s at pc 0x%08X - resetting\n", TypeText(m_record.type), m_record.pc);

#if CRASH_HALT_ON_FAULT
        NRF_BREAKPOINT_COND;
#endif

        NVIC_SystemReset();
    }
}

namespace svc
{
    void CrashRecord::ReportAtBoot()
    {
        // RESETREAS is sticky: clear the bits by writing them back, or every later
        // boot inherits this one's reasons.
        const uint32_t reasons = NRF_POWER->RESETREAS;
        NRF_POWER->RESETREAS = reasons;

        if (m_counters.magic != COUNTER_MAGIC)
        {
            m_counters.magic = COUNTER_MAGIC;
            m_counters.boots = 0;
            m_counters.crashes = 0;
        }

        m_counters.boots++;
        m_counters.heartbeat = 0;

        LOG_WARNING("Boot %d: reset reason 0x%08X (%s)\n",
                    m_counters.boots, reasons, ResetReasonText(reasons));

        if (m_record.magic == RECORD_MAGIC)
        {
            // Passing the RAM buffers straight to %s is safe only because logging
            // is in-place (NRF_LOG_DEFERRED 0), so the message is formatted before
            // these calls return. Deferred logging would need nrf_log_push().
            LOG_ERROR("CRASH RECORD: %s in task '%s', %d heartbeats into the run\n",
                      TypeText(m_record.type), m_record.task, m_record.heartbeat);

            if (m_record.type == TYPE_SDK_ERROR)
            {
                LOG_ERROR("  error %d [%s] at %s:%d\n",
                          m_record.err_code,
                          nrf_strerror_get(m_record.err_code),
                          m_record.file,
                          m_record.line);
            }
            else if (m_record.type == TYPE_SDK_ASSERT)
            {
                LOG_ERROR("  assertion failed at %s:%d\n", m_record.file, m_record.line);
            }

            LOG_ERROR("  pc 0x%08X lr 0x%08X psr 0x%08X\n",
                      m_record.pc, m_record.lr, m_record.psr);

            if (m_record.type == TYPE_HARD_FAULT)
            {
                LOG_ERROR("  cfsr 0x%08X hfsr 0x%08X mmfar 0x%08X bfar 0x%08X\n",
                          m_record.cfsr, m_record.hfsr, m_record.mmfar, m_record.bfar);
            }

            LOG_ERROR("  crashes since power-on: %d\n", m_counters.crashes);

            m_record.magic = 0;
        }
        else
        {
            LOG_INFO("No crash record (crashes since power-on: %d)\n", m_counters.crashes);
        }

        PaintMainStack();
    }

    void CrashRecord::NoteHeartbeat(uint32_t heartbeat)
    {
        if (m_counters.magic == COUNTER_MAGIC)
        {
            m_counters.heartbeat = heartbeat;
        }
    }

    void CrashRecord::LogStackHeadroom()
    {
        LOG_INFO("Stack free: main/ISR %d B\n", MainStackFreeBytes());

        for (uint8_t i = 0; i < eda::ActiveObject::TaskCount(); i++)
        {
            TaskHandle_t const task = eda::ActiveObject::TaskAt(i);

            if (task == nullptr)
            {
                continue;
            }

            LOG_INFO("Stack free: %s %d B\n",
                     pcTaskGetName(task),
                     static_cast<uint32_t>(uxTaskGetStackHighWaterMark(task) * sizeof(StackType_t)));
        }

        TaskHandle_t const timer_task = xTimerGetTimerDaemonTaskHandle();
        TaskHandle_t const idle_task = xTaskGetIdleTaskHandle();

        if (timer_task != nullptr)
        {
            LOG_INFO("Stack free: %s %d B\n",
                     pcTaskGetName(timer_task),
                     static_cast<uint32_t>(uxTaskGetStackHighWaterMark(timer_task) * sizeof(StackType_t)));
        }

        if (idle_task != nullptr)
        {
            LOG_INFO("Stack free: %s %d B\n",
                     pcTaskGetName(idle_task),
                     static_cast<uint32_t>(uxTaskGetStackHighWaterMark(idle_task) * sizeof(StackType_t)));
        }
    }
}

/// Strong override of the SDK's weak handler (app_error_weak.c). Everything that
/// calls APP_ERROR_CHECK, ASSERT, NRFX_ASSERT or FreeRTOS configASSERT arrives
/// here, as do SoftDevice assertions and memory-access faults.
extern "C" void app_error_fault_handler(uint32_t id, uint32_t pc, uint32_t info)
{
    __disable_irq();

    if (StartRecord(FaultType(id)))
    {
        m_record.pc = pc;

        switch (id)
        {
        case NRF_FAULT_ID_SDK_ERROR:
            if (IsReadable(info, sizeof(error_info_t)))
            {
                error_info_t const *const p_info = reinterpret_cast<error_info_t const *>(info);
                m_record.line = p_info->line_num;
                m_record.err_code = p_info->err_code;
                CopyName(m_record.file, FILE_LEN, BaseName(p_info->p_file_name));
            }
            break;

        case NRF_FAULT_ID_SDK_ASSERT:
            if (IsReadable(info, sizeof(assert_info_t)))
            {
                assert_info_t const *const p_info = reinterpret_cast<assert_info_t const *>(info);
                m_record.line = p_info->line_num;
                CopyName(m_record.file, FILE_LEN, BaseName(p_info->p_file_name));
            }
            break;

        default:
            m_record.err_code = info;
            break;
        }

        CommitRecord();
    }

    FinishFault();
}

/// Strong override of the weak handler in ses_startup_nrf52840.s. The SDK's own
/// hardfault library is switched off (HARDFAULT_HANDLER_ENABLED 0) because its
/// Debug path executes a breakpoint before any of this would run.
extern "C" __attribute__((naked)) void HardFault_Handler(void)
{
    // Pick the stack the exception frame was pushed on: EXC_RETURN bit 2 set means
    // the interrupted code was running on the process stack.
    __asm volatile(
        "   tst lr, #4                  \n"
        "   ite eq                      \n"
        "   mrseq r0, msp               \n"
        "   mrsne r0, psp               \n"
        "   b crash_record_hard_fault   \n");
}

extern "C" void crash_record_hard_fault(uint32_t *p_frame)
{
    __disable_irq();

    if (StartRecord(svc::CrashRecord::TYPE_HARD_FAULT))
    {
        m_record.cfsr = SCB->CFSR;
        m_record.hfsr = SCB->HFSR;
        m_record.mmfar = SCB->MMFAR;
        m_record.bfar = SCB->BFAR;

        // A blown stack is one of the ways to end up here, so the frame pointer
        // itself cannot be trusted.
        if (IsReadable(reinterpret_cast<uint32_t>(p_frame), 8u * sizeof(uint32_t)))
        {
            m_record.lr = p_frame[5];
            m_record.pc = p_frame[6];
            m_record.psr = p_frame[7];
        }

        CommitRecord();
    }

    FinishFault();
}

/// Called by the kernel when configCHECK_FOR_STACK_OVERFLOW catches a task
/// running past the end of its stack.
extern "C" void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;

    __disable_irq();

    if (StartRecord(svc::CrashRecord::TYPE_STACK_OVERFLOW))
    {
        CopyName(m_record.task, TASK_LEN, pcTaskName);
        CommitRecord();
    }

    FinishFault();
}
