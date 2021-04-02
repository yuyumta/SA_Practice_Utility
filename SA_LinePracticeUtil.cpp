#include "plugin.h"
#include "CTimer.h"
#include "CHud.h"
#include "CCamera.h"
#include "extensions/ScriptCommands.h"
using namespace plugin;

class LinePracticeUtil {

public:

    LinePracticeUtil() {
        static CVector position;
        static CVector speed;
        static CVector camera;
        static float ux, uy, uz, rx, ry, rz;
        static int m_nLastSpawnedTime = 0;
        static float cx, cy, cz;

        CHud::SetHelpMessage("Saved", true, false, false);
        Events::gameProcessEvent += [] {
            CVehicle* playerVehicle = FindPlayerVehicle(-1, false);
            CPhysical* entity = (CPhysical*)FindPlayerEntity(-1);
            CPlayerPed* player = FindPlayerPed(-1);


            if (KeyPressed(0x31) && (playerVehicle) && CTimer::m_snTimeInMilliseconds > (m_nLastSpawnedTime + 1000)) {

                CVehicle* playerVehicle = FindPlayerVehicle(-1, false);
                CPhysical* entity = (CPhysical*)FindPlayerEntity(-1);
                CPlayerPed* player = FindPlayerPed(-1);

                CHud::SetHelpMessage("Loaded", true, false, false);
                m_nLastSpawnedTime = CTimer::m_snTimeInMilliseconds;

                entity->Teleport(position, false);
                playerVehicle->m_vecMoveSpeed = speed;
                player->m_nPedFlags.CantBeKnockedOffBike = 0; // so you can warp back to your savepoint after driving into water
                Command<eScriptCommands::COMMAND_WARP_CHAR_INTO_CAR>(player, playerVehicle); // so the sound starts working after you drive a bike into water

                playerVehicle->m_matrix->up.x = ux;
                playerVehicle->m_matrix->up.y = uy;
                playerVehicle->m_matrix->up.z = uz;
                playerVehicle->m_matrix->right.x = rx;
                playerVehicle->m_matrix->right.y = ry;
                playerVehicle->m_matrix->right.z = rz;

                playerVehicle->Fix();
                playerVehicle->m_fHealth = 10000.0f;
                Command<eScriptCommands::COMMAND_RESTORE_CAMERA_JUMPCUT>();
            }
            if (KeyPressed(VK_TAB) && (playerVehicle) && (CTimer::m_snTimeInMilliseconds > (m_nLastSpawnedTime + 1000))) {

                CVehicle* playerVehicle = FindPlayerVehicle(-1, false);
                CPhysical* entity = (CPhysical*)FindPlayerEntity(-1);
                CPlayerPed* player = FindPlayerPed(-1);

                CHud::SetHelpMessage("Set", true, false, false);
                m_nLastSpawnedTime = CTimer::m_snTimeInMilliseconds;

                position = entity->GetPosition();
                speed = playerVehicle->m_vecMoveSpeed;

                ux = playerVehicle->m_matrix->up.x;
                uy = playerVehicle->m_matrix->up.y;
                uz = playerVehicle->m_matrix->up.z;
                rx = playerVehicle->m_matrix->right.x;
                ry = playerVehicle->m_matrix->right.y;
                rz = playerVehicle->m_matrix->right.z;
            }
            if (KeyPressed(0x31) && (!playerVehicle) && (CTimer::m_snTimeInMilliseconds > (m_nLastSpawnedTime + 1000))) {

                for (CVehicle* vehicle : CPools::ms_pVehiclePool)
                {
                    if (vehicle == player->m_pVehicle) {

                        player->m_nPedFlags.CantBeKnockedOffBike = 1; // water bike
                        Command<eScriptCommands::COMMAND_WARP_CHAR_INTO_CAR>(player, vehicle);

                }
                }

            }

        };
    }
}
LinePracticeUtil;
