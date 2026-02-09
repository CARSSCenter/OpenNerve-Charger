#include "../../application_layer/app_system.h"

int main()
{
    app::System &systemInstance = app::System::GetInstance();
    systemInstance.Init();
    systemInstance.Run();

    return 0;
}
