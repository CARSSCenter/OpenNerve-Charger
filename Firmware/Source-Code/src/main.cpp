#include "../../application_layer/app_system.h"


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
    disable_nfc_pins_if_needed();
    app::System &systemInstance = app::System::GetInstance();
    systemInstance.Init();
    systemInstance.Run();

    return 0;
}
