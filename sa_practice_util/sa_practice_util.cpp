#include "plugin.h"
#include "common.h"
#include "CTimer.h"
#include "CHud.h"
#include "CCamera.h"
#include "CStats.h"
#include "extensions/ScriptCommands.h"
#include "CMenuManager.h"
#include "INIReader.h"
#include <iostream>
#include "CGenericGameStorage.h"
#include "CTheCarGenerators.h"
#include <fstream>
#include <filesystem>
#include "CGame.h"
#include "CClock.h"
#include "CWeather.h"


using namespace plugin;
using namespace std;

// the reason this exists is cause i dont believe the included plugin-sdk function to generate a parked vehicle has an input for a few essential variables, so i just patch the save file manually
// if i remember correctly, the value i change is one related to game progression (the same value which determines whether you get a hydra in grove street, for example). by default, its set so
// none of your parked vehicles show up on a new game
void SavePatchThing(const char* filename, uint16_t compare_id)
{
	ifstream infile(filename, ios::in | ios::out | ios::binary);
	ofstream outfile(filename, ios::in | ios::out | ios::binary);

	int input;
	int match = 0;
	int found;
	int length = { 0 };
	uint16_t id;
	uint16_t unknown;
	uint32_t checksum = 0;

	while (!infile.eof())
	{
		input = infile.get();
		if (input == 0x42)
		{

			infile.read(reinterpret_cast<char*>(&found), 4);
			if (found == 0x4B434F4C)
			{
				match += 1;


				length = (int)infile.tellg();
				if (match == 13)
				{

					for (int i = 0; i < 252; i++)
					{
						infile.seekg(length + 8 + 34 * i);
						infile.read(reinterpret_cast<char*>(&id), 2);
						infile.seekg(length + 33 + 34 * i);
						infile.read(reinterpret_cast<char*>(&unknown), 2);
						if (i < 50 && id == compare_id || i > 207 && id == compare_id)
						{
							if (id > 300 && id < 700)
							{
								outfile.seekp(length + 33 + 34 * i);
								//outfile.write((const char*)0x0000, 2);
								outfile.put(0xFF);
								outfile.put(0xFF);

							}
						}
					}

				}
			}
		}


		length++;
	}
	length = { 0 };
	infile.clear();
	infile.seekg(0);
	while (length < 202748)
	{
		checksum += infile.get();
		length++;
	}
	//length += 1;
	outfile.seekp(length);
	outfile.put(checksum & 0xff);
	outfile.put((checksum >> 8) & 0xff);
	outfile.put((checksum >> 16) & 0xff);
	outfile.put((checksum >> 24) & 0xff);
}

class LinePracticeUtilSAV2 {
public:


	LinePracticeUtilSAV2() {



		INIReader reader("SAPU-config.ini");

		static int setkey = reader.GetInteger("binds", "setkey", 0x09);
		static int loadkey = reader.GetInteger("binds", "loadkey", 0x11);
		static int quicksavekey = reader.GetInteger("binds", "quicksavekey", 0x11);
		static int quickloadkey = reader.GetInteger("binds", "quickloadkey", 0x11);
		static int vehiclegenkey = reader.GetInteger("binds", "vehiclegenkey", 0x11);
		static bool healplayer = reader.GetInteger("options", "set_player_health", 0);
		static bool healvehicle = reader.GetInteger("options", "set_vehicle_health", 0);
		static bool undoskill = reader.GetInteger("options", "set_vehicle_skill", 0);
		static bool undotime = reader.GetInteger("options", "set_time", 0);
		static bool quicksave = reader.GetInteger("options", "enable_quicksave", 0);

		static CVector position;
		static CVector speed;
		static CVector camera;
		static float ux, uy, uz, rx, ry, rz;
		static int m_nLastSpawnedTime = 0;
		static float cx, cy, cz;
		static float vhealth;
		static float phealth;
		static float parmour;
		static int cycle_skill, cycle_stamina, driving_skill, flying_skill, boat_skill, bike_skill;
		static char hours, minutes, day;
		static short weather;

		static bool paused;
		static char *gamePath = reinterpret_cast<char*>(0xC92368);
		static uint16_t generator_vehicle_id;

		Events::gameProcessEvent += [] {

			CVehicle* playerVehicle = FindPlayerVehicle(-1, false);
			CPhysical* entity = (CPhysical*)FindPlayerEntity(-1);
			CPlayerPed* player = FindPlayerPed(-1);

			if (!CTimer::m_UserPause && KeyPressed(0x31) && CTimer::m_snTimeInMilliseconds - m_nLastSpawnedTime > 500) {
				CPlayerPed* player = FindPlayerPed(-1);
				m_nLastSpawnedTime = CTimer::m_snTimeInMilliseconds;
				char idk[10];
				std::sprintf(idk, "%i", player->m_nPedFlags.CantBeKnockedOffBike);
				CHud::SetHelpMessage(idk, true, false, false);
				

			}
			if (!CTimer::m_UserPause && KeyPressed(0x32) && CTimer::m_snTimeInMilliseconds - m_nLastSpawnedTime > 500) {
				CPlayerPed* player = FindPlayerPed(-1);
				m_nLastSpawnedTime = CTimer::m_snTimeInMilliseconds;
				player->m_nPedFlags.CantBeKnockedOffBike = 1;


			}
			if (!CTimer::m_UserPause && KeyPressed(0x33) && CTimer::m_snTimeInMilliseconds - m_nLastSpawnedTime > 500) {
				CPlayerPed* player = FindPlayerPed(-1);
				m_nLastSpawnedTime = CTimer::m_snTimeInMilliseconds;
				player->m_nPedFlags.CantBeKnockedOffBike = 0;


			}

			if (playerVehicle && !CTimer::m_UserPause && KeyPressed(loadkey) && CTimer::m_snTimeInMilliseconds - m_nLastSpawnedTime > 500 && ux) {

				m_nLastSpawnedTime = CTimer::m_snTimeInMilliseconds;

				CVehicle* playerVehicle = FindPlayerVehicle(-1, false);
				CPhysical* entity = (CPhysical*)FindPlayerEntity(-1);
				CPlayerPed* player = FindPlayerPed(-1);
				entity->Teleport(position, false);

				CHud::SetHelpMessage("Loaded", true, false, false);

				player->m_nPedFlags.CantBeKnockedOffBike = 0; // so you can warp back to your savepoint after driving into water
				Command<eScriptCommands::COMMAND_WARP_CHAR_INTO_CAR>(player, playerVehicle); // so the sound starts working after you drive a bike into water

				playerVehicle->m_matrix->up.x = ux;
				playerVehicle->m_matrix->up.y = uy;
				playerVehicle->m_matrix->up.z = uz;
				playerVehicle->m_matrix->right.x = rx;
				playerVehicle->m_matrix->right.y = ry;
				playerVehicle->m_matrix->right.z = rz;
				playerVehicle->m_vecMoveSpeed = speed;

				if (healvehicle) {
					playerVehicle->Fix();
					playerVehicle->m_fHealth = vhealth;
				}

				if (healplayer) {
					player->m_fHealth = phealth;
					player->m_fArmour = parmour;
				}

				if (undoskill)
				{
					CStats::m_CycleSkillCounter = cycle_skill;
					CStats::m_CycleStaminaCounter = cycle_stamina;
					CStats::m_DrivingCounter = driving_skill;
					CStats::m_FlyingCounter = flying_skill;
					CStats::m_BoatCounter = boat_skill;
					CStats::m_BikeCounter = bike_skill;
				}
				if (undotime)
				{
					CClock::SetGameClock(hours, minutes, day);
					//CClock::RestoreClock();
					CWeather::ForceWeather(weather);
				}

				Command<eScriptCommands::COMMAND_RESTORE_CAMERA_JUMPCUT>();
			}

			// set vehicle position, speed, heading, etc
			if (playerVehicle && !CTimer::m_UserPause && KeyPressed(setkey) && CTimer::m_snTimeInMilliseconds - m_nLastSpawnedTime > 500) {

				m_nLastSpawnedTime = CTimer::m_snTimeInMilliseconds;

				CVehicle* playerVehicle = FindPlayerVehicle(-1, false);
				CPhysical* entity = (CPhysical*)FindPlayerEntity(-1);
				CPlayerPed* player = FindPlayerPed(-1);

				CHud::SetHelpMessage("Loaded", true, false, false);


				position = entity->GetPosition();
				speed = playerVehicle->m_vecMoveSpeed;
				vhealth = playerVehicle->m_fHealth;
				phealth = player->m_fHealth;
				parmour = player->m_fArmour;
				cycle_skill = CStats::m_CycleSkillCounter;
				cycle_stamina = CStats::m_CycleStaminaCounter;
				driving_skill = CStats::m_DrivingCounter;
				flying_skill = CStats::m_FlyingCounter;
				boat_skill = CStats::m_BoatCounter;
				bike_skill = CStats::m_BikeCounter;
				//CClock::StoreClock();
				hours = CClock::ms_nGameClockHours;
				minutes = CClock::ms_nGameClockMinutes;
				day = CClock::CurrentDay;
				weather = CWeather::NewWeatherType;

				ux = playerVehicle->m_matrix->up.x;
				uy = playerVehicle->m_matrix->up.y;
				uz = playerVehicle->m_matrix->up.z;
				rx = playerVehicle->m_matrix->right.x;
				ry = playerVehicle->m_matrix->right.y;
				rz = playerVehicle->m_matrix->right.z;
			}
			if (!playerVehicle && !CTimer::m_UserPause && KeyPressed(loadkey) && CTimer::m_snTimeInMilliseconds - m_nLastSpawnedTime > 500) {

				for (CVehicle* vehicle : CPools::ms_pVehiclePool)
				{
					if (vehicle == player->m_pVehicle) {

						player->m_nPedFlags.CantBeKnockedOffBike = 1; // water bike
						Command<eScriptCommands::COMMAND_WARP_CHAR_INTO_CAR>(player, vehicle);

					}
				}

			}

			// vehicle to generator
			if (playerVehicle && quicksave && !CTimer::m_UserPause && KeyPressed(vehiclegenkey) && CTimer::m_snTimeInMilliseconds - m_nLastSpawnedTime > 500) {
				m_nLastSpawnedTime = CTimer::m_snTimeInMilliseconds;

				CVehicle* playerVehicle = FindPlayerVehicle(-1, false);
				CVector pos = playerVehicle->GetPosition();
				generator_vehicle_id = playerVehicle->m_nModelIndex;

				float heading = 0;
				heading = atan2(-(playerVehicle->m_matrix->up.x), playerVehicle->m_matrix->up.y) * (180 / 3.14159);
				if (heading < 0)
				{
					heading = 360 + heading;
				}
				//char idk[10];
				//sprintf(idk, "%f", heading);
				CHud::SetHelpMessage("Set vehicle", true, false, false);

				CTheCarGenerators::CreateCarGenerator(pos.x, pos.y, pos.z, heading, generator_vehicle_id, -1, -1, true, 0, 0, 0, 10000, 0, true);
				

			}


			// borrowed some code for quicksaves/quickloads from Lordmau5's chaos mod, so, uhm, thanks :^)
			// quickload
			if (!CTimer::m_UserPause && quicksave && KeyPressed(quickloadkey) && CTimer::m_snTimeInMilliseconds - m_nLastSpawnedTime > 500) {
				m_nLastSpawnedTime = CTimer::m_snTimeInMilliseconds;
				sprintf(CGenericGameStorage::ms_LoadFileName, "%s\\%s", gamePath, "SAPU-quicksave.b");

				if (filesystem::exists(CGenericGameStorage::ms_LoadFileName))
				{
					FrontEndMenuManager.m_bLoadingData = true;
					FrontEndMenuManager.m_bMenuActive = false;
					BYTE gameState = injector::ReadMemory<BYTE>(0xC8D4C0, true);
					//					char idk[10];
					//					sprintf(idk, "%d", gameState);
					//					CHud::SetHelpMessage(idk, true, false, false);
					if (gameState == 9)
					{
						CGame::ShutDownForRestart();
						CGame::InitialiseWhenRestarting();
					}
				}
				else
				{
					CHud::SetHelpMessage("Unable to find quicksave", true, false, false);
				}
			}


			// quicksave
			if (!CTimer::m_UserPause && quicksave && KeyPressed(quicksavekey) && CTimer::m_snTimeInMilliseconds - m_nLastSpawnedTime > 500) {
				if (!playerVehicle)
				{
					m_nLastSpawnedTime = CTimer::m_snTimeInMilliseconds;
					sprintf(CGenericGameStorage::ms_ValidSaveName, "%s\\%s", gamePath, "SAPU-quicksave.b");
					CGenericGameStorage::GenericSave(0);
					// twice cause I'm too lazy to figure out a bug and it seems to work fine like this
					SavePatchThing((const char*)CGenericGameStorage::ms_ValidSaveName, generator_vehicle_id);
					SavePatchThing((const char*)CGenericGameStorage::ms_ValidSaveName, generator_vehicle_id);
					CHud::SetHelpMessage("Quicksaved", true, false, false);
				}
				else
				{
					CHud::SetHelpMessage("Cannot save inside of vehicles", true, false, false);
				}
			}
		};
	}
} linePracticeUtilSAV2;
