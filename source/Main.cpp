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
#include "CFont.h"
#include "CCheckpoints.h"
#include "CAudioEngine.h"
#include "CPickups.h"


using namespace plugin;
using namespace std;

// the reason this exists is cause i dont believe the included plugin-sdk function to generate a parked vehicle has an input for a few essential variables, so i just patch the save file manually
// if i remember correctly, the value i change is one related to game progression (the same value which determines whether you get a hydra in grove street, for example). 
// by default, its set so none of your parked vehicles show up on a new game
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


class sapu {
public:
    sapu() {
		INIReader reader("sapu-config.ini");
		static int setkey = reader.GetInteger("binds", "setkey", 0xA4);
		static int loadkey = reader.GetInteger("binds", "loadkey", 0x58);
		static int quicksavekey = reader.GetInteger("binds", "quicksavekey", 0x74);
		static int quickloadkey = reader.GetInteger("binds", "quickloadkey", 0x78);
		static int vehiclegenkey = reader.GetInteger("binds", "vehiclegenkey", 0x75);
		static int feetkey = reader.GetInteger("binds", "onfoottogglekey", 0x91);
		static int checkpointtogglekey = reader.GetInteger("binds", "checkpointtogglekey", 0x7B);
		static int checkpointplacekey = reader.GetInteger("binds", "checkpointplacekey", 0x5A);

		static bool healplayer = reader.GetInteger("options", "set_player_health", 1);
		static bool healvehicle = reader.GetInteger("options", "set_vehicle_health", 1);
		static bool undoskill = reader.GetInteger("options", "set_vehicle_skill", 1);
		static bool undotime = reader.GetInteger("options", "set_time", 1);
		static bool quicksave = reader.GetInteger("options", "enable_quicksave", 1);

		// vehicle data
		static CVector vehicle_u, vehicle_r, vehicle_position, vehicle_speed;
		static float vehicle_health;
		static int m_nLastSpawnedTime = 0;
		static uint16_t generator_vehicle_id;

		// player data
		static CVector player_u, player_r, player_at, player_position, player_speed,camera;
		static float player_health;
		static float player_armour;
		static int cycle_skill, cycle_stamina, driving_skill, flying_skill, boat_skill, bike_skill;

		// general data
		static bool feet = false;
		static char hours, minutes, day;
		static short weather;
		static bool paused;
		static char* gamePath = reinterpret_cast<char*>(0xC92368);
		static bool checkpoint_mode = false;
		static bool timing = false;
		static float timer = 0.0;
		static CCheckpoint* cp;




		// print timer when cp mode
		Events::drawingEvent += [] {
			if (checkpoint_mode) {
				CFont::SetOrientation(ALIGN_LEFT);
				CFont::SetColor(CRGBA(255, 255, 255, 255));
				CFont::SetBackground(false, true);
				CFont::SetDropShadowPosition(1);
				CFont::SetWrapx(500.0);
				CFont::SetScale(1, 2);
				CFont::SetFontStyle(FONT_SUBTITLES);
				CFont::SetProportional(true);
				char text[1024];
				int mil = fmod(timer/10000000, 1) * 1000;
				int sec = fmod(timer / 10000000, 60);
				int min = fmod(timer / 10000000, 60 * 60) / 60;
				// 00:0.0 format
				sprintf(text, "%02d:%02d.%03d", min, sec, mil);//(int)timer/10000000/60, timer/10000000);
				//sprintf(text, "%02d", int(timer/10000000/60));
				CFont::PrintString(75, 10, text);
			}
		};

		Events::gameProcessEvent += [] {

			CVehicle* playerVehicle = FindPlayerVehicle(-1, false);
			CPhysical* entity = (CPhysical*)FindPlayerEntity(-1);
			CPlayerPed* player = FindPlayerPed(-1);

			// while timer is running
			if (timing)
			{
				if (cp)
				{
					timer = timer + CTimer::m_snTimeInMilliseconds / CTimer::game_FPS * 1.9;
					if (Command<eScriptCommands::COMMAND_LOCATE_CHAR_ANY_MEANS_2D>(player, cp->m_vecPosition.x, cp->m_vecPosition.y, 4.0, 4.0, true)) {
						timing = false;
						Command<eScriptCommands::COMMAND_ADD_ONE_OFF_SOUND>(player->m_matrix->pos.x, player->m_matrix->pos.y, player->m_matrix->pos.z, 1056);
						
					}
				}

			}

			// place cp (limited to one)
			if (!CTimer::m_UserPause && KeyPressed(checkpointplacekey) && CTimer::m_snTimeInMilliseconds - m_nLastSpawnedTime > 500 && checkpoint_mode) {
				m_nLastSpawnedTime = CTimer::m_snTimeInMilliseconds;
				cp = CCheckpoints::PlaceMarker(254, 1, player->m_matrix->pos, CVector(0.0, 0.0, 0.0), 2.0, 255, 0, 0, 255, 0, 0.0, 0);
			}

			//cp mode toggle
			if (!CTimer::m_UserPause && KeyPressed(checkpointtogglekey) && CTimer::m_snTimeInMilliseconds - m_nLastSpawnedTime > 500) {
				m_nLastSpawnedTime = CTimer::m_snTimeInMilliseconds;
				checkpoint_mode = !checkpoint_mode;
				timing = false;
				if (!checkpoint_mode) {
					CCheckpoints::DeleteCP(254, 1);
					cp = 0;
					timer = 0.0;

				}
	
			}

			//feet toggle
			if (!CTimer::m_UserPause && KeyPressed(feetkey) && CTimer::m_snTimeInMilliseconds - m_nLastSpawnedTime > 500) {
				m_nLastSpawnedTime = CTimer::m_snTimeInMilliseconds;
				if (feet) {
					feet = false;
					CHud::SetHelpMessage("Vehicle Mode", true, false, false);
				}
				else {
					feet = true;
					CHud::SetHelpMessage("On-Foot Mode", true, false, false);
				}
			}


			if (!CTimer::m_UserPause && KeyPressed(loadkey) && !playerVehicle) {
				// so you can tp back into a car from on foot
				for (CVehicle* vehicle : CPools::ms_pVehiclePool)
				{
					if (vehicle == player->m_pVehicle) {
						player->m_nPedFlags.CantBeKnockedOffBike = 1; // water bike
						Command<eScriptCommands::COMMAND_WARP_CHAR_INTO_CAR>(player, vehicle);
						if (!vehicle_u.x) {
							player->m_nPedFlags.CantBeKnockedOffBike = 0;
						}


					}

				}
			}

			// restore pos/vehicle/stuff
			if (!CTimer::m_UserPause && KeyPressed(loadkey) && CTimer::m_snTimeInMilliseconds - m_nLastSpawnedTime > 500) {
				m_nLastSpawnedTime = CTimer::m_snTimeInMilliseconds;
				CPlayerPed* player = FindPlayerPed(-1);
				CVehicle* playerVehicle = FindPlayerVehicle(-1, false);
				CPhysical* entity = (CPhysical*)FindPlayerEntity(-1);

				if (playerVehicle && vehicle_u.x && !feet) {
					
					entity->Teleport(vehicle_position, false);


					player->m_nPedFlags.CantBeKnockedOffBike = 0; // so you can warp back to your savepoint after driving into water
					Command<eScriptCommands::COMMAND_WARP_CHAR_INTO_CAR>(player, playerVehicle); // so the sound starts working after you drive a bike into water

					playerVehicle->m_matrix->up = vehicle_u;
					playerVehicle->m_matrix->right = vehicle_r;
					playerVehicle->m_vecMoveSpeed = vehicle_speed;

					if (healvehicle) {
						playerVehicle->Fix();
						playerVehicle->m_fHealth = vehicle_health;
					}

					if (healplayer) {
						player->m_fHealth = player_health;
						player->m_fArmour = player_armour;
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

					if (checkpoint_mode)
					{
						timing = true;
						timer = 0.0;
					}

					Command<eScriptCommands::COMMAND_RESTORE_CAMERA_JUMPCUT>();

				}
				else if (player_position.x && feet)
				{
					player->Teleport(player_position, false);
					if (healvehicle) {
						playerVehicle->Fix();
						playerVehicle->m_fHealth = vehicle_health;
					}

					if (healplayer) {
						player->m_fHealth = player_health;
						player->m_fArmour = player_armour;
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

			}




			// save position, speed, heading, etc
			if (!CTimer::m_UserPause && KeyPressed(setkey) && CTimer::m_snTimeInMilliseconds - m_nLastSpawnedTime > 500) {

				m_nLastSpawnedTime = CTimer::m_snTimeInMilliseconds;

				CVehicle* playerVehicle = FindPlayerVehicle(-1, false);
				CPhysical* entity = (CPhysical*)FindPlayerEntity(-1);
				CPlayerPed* player = FindPlayerPed(-1);

				if (playerVehicle && !feet)
				{

					vehicle_position = entity->GetPosition();
					vehicle_speed = playerVehicle->m_vecMoveSpeed;
					vehicle_health = playerVehicle->m_fHealth;
					player_health = player->m_fHealth;
					player_armour = player->m_fArmour;
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

					vehicle_u = playerVehicle->m_matrix->up;
					vehicle_r = playerVehicle->m_matrix->right;
					CHud::SetHelpMessage("Set", true, false, false);
				}
				else if (feet)
				{
					player_position = player->m_matrix->pos;
					player_u = player->m_matrix->up;
					player_r = player->m_matrix->right;
					player_at = player->m_matrix->at;

					CHud::SetHelpMessage("Set", true, false, false);
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
					FrontEndMenuManager.m_bWantToLoad = true;
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
					m_nLastSpawnedTime = CTimer::m_snTimeInMilliseconds;
					CHud::SetHelpMessage("Cannot save inside of vehicles", true, false, false);
				}
			}
		};
	}
} sapuPlugin;
