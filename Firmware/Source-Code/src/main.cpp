#include "../../application_layer/app_system.h"

#include "nrf_erratas.h"

/// Keeps the SWD debug port open across resets on revision-3 nRF52840 silicon
/// (FICR INFO.VARIANT "AAF0"; the SDK's errata-249 check).
///
/// Those parts re-arm access-port protection on every reset unless firmware
/// opens it again at each boot. The MDK's system_nrf52840.c never does (only the
/// generic system_nrf52.c calls nrf52_handle_approtect()), so after any power-on,
/// brown-out or pin reset the port was locked. RTT then went silent and the next
/// J-Link connect "unsecured" the chip - which is a full erase. That, not a
/// firmware crash, is why the board seemed to need re-flashing after a reset.
///
/// Debug builds only: a production charger keeps its debug port locked.
static void keep_debug_port_open(void)
{
#if defined(DEBUG)
    if (!nrf52_errata_249())
    {
        return; // older silicon: protection is controlled by UICR alone
    }

    // The hardware half: UICR.APPROTECT must read HwDisabled. It is only
    // written while still erased, because UICR bits cannot be set back to 1
    // without an erase - a locked value here would otherwise reset forever.
    if (NRF_UICR->APPROTECT == 0xFFFFFFFFUL)
    {
        NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen;
        while (NRF_NVMC->READY == NVMC_READY_READY_Busy) { }

        NRF_UICR->APPROTECT = UICR_APPROTECT_PALL_HwDisabled;
        while (NRF_NVMC->READY == NVMC_READY_READY_Busy) { }

        NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren;
        while (NRF_NVMC->READY == NVMC_READY_READY_Busy) { }

        // UICR is only sampled at reset.
        NVIC_SystemReset();
    }

    // The software half, needed after every reset.
    NRF_APPROTECT->DISABLE = APPROTECT_DISABLE_DISABLE_SwDisable;
#endif
}


static void disable_nfc_pins_if_needed(void)
{   
    //check if NFC pins are enabled
    if (NRF_UICR->NFCPINS == 0) {
        return; 
    }
    //enable write to NVMC
    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen;
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy) { }
    //disable NFC pins
    NRF_UICR->NFCPINS = 0;

    while (NRF_NVMC->READY == NVMC_READY_READY_Busy) { }
    //disable write to NVMC
    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren;
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy) { }

    NVIC_SystemReset();
}

int main()
{
    // First, so the debug port is open even if anything later hangs.
    keep_debug_port_open();
    disable_nfc_pins_if_needed();
    app::System &systemInstance = app::System::GetInstance();
    systemInstance.Init();
    systemInstance.Run();

    return 0;
}
