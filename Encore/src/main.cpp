﻿
#if defined(WIN32) && defined(NDEBUG)
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
#endif
#include "rapidjson/document.h"
#include "rapidjson/filewritestream.h"
#include "rapidjson/writer.h"
#include "raylib.h"
#include <vector>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <algorithm>
#include "song/song.h"
#include "song/songlist.h"
#include "audio/audio.h"
#include "game/arguments.h"
#include "game/utility.h"
#include "game/player.h"
#include "game/keybinds.h"
#include "game/assets.h"
#include "game/settings.h"
#include "raygui.h"

#include <stdlib.h>
#include "GLFW/glfw3.h"
#include <cstdio>
vector<std::string> ArgumentList::arguments;

static bool compareNotes(const Note& a, const Note& b) {
	return a.time < b.time;
}



bool midiLoaded = false;
bool isPlaying = false;
bool streamsLoaded = false;

std::vector<std::pair<Music, int>> loadedStreams;
int curPlayingSong = 0;
std::vector<int> curNoteIdx = { 0,0,0,0,0 };
int curODPhrase = 0;
int curBeatLine = 0;
int curBPM = 0;
int selLane = 0;
int songSelectOffset = 0;
bool changingKey = false;
bool changingOverdrive = false;
double startedPlayingSong = 0.0;
Vector2 viewScroll = { 0,0 };
Rectangle view = { 0 };


std::string trackSpeedButton;

enum Screens {
	MENU,
	SONG_SELECT,
	INSTRUMENT_SELECT,
	DIFFICULTY_SELECT,
	GAMEPLAY,
	RESULTS,
	SETTINGS
};

int currentScreen = MENU;

std::vector<bool> heldFrets{ false,false,false,false,false };
std::vector<bool> heldFretsAlt{ false,false,false,false,false };
std::vector<bool> overhitFrets{ false,false,false,false,false };
std::vector<bool> tapRegistered{ false,false,false,false,false };
std::vector<bool> liftRegistered{ false,false,false,false,false };
bool overdriveHeld = false;
bool overdriveAltHeld = false;
bool overdriveHitAvailable = false;
bool overdriveLiftAvailable = false;
std::vector<bool> overdriveLanesHit{ false,false,false,false,false };
double overdriveHitTime = 0.0;
std::vector<int> lastHitLifts{-1, -1, -1, -1, -1};
SongList songList;
Settings settings;
Assets assets;
double lastAxesTime = 0.0;
std::vector<float> axesValues{0.0f,0.0f,0.0f,0.0f,0.0f,0.0f};
std::vector<int> buttonValues{ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
std::vector<float> axesValues2{ 0.0f,0.0f,0.0f,0.0f,0.0f,0.0f };
int pressedGamepadInput = -999;
int axisDirection = -1;
int controllerID = -1;

static void DrawTextRubik(const char* text, float posX, float posY, int fontSize, Color color) {
	DrawTextEx(assets.rubik, text, { posX,posY }, fontSize, 2, color);
}
static void DrawTextRHDI(const char* text, float posX, float posY, Color color) {
    DrawTextEx(assets.redHatDisplayItalic, text, { posX,posY }, 48, 2, color);
}

static int MeasureTextRubik(const char* text, int fontSize) {
	return MeasureTextEx(assets.rubik, text, fontSize, 2).x;
}
static int MeasureTextRHDI(const char* text) {
    return MeasureTextEx(assets.rubik, text, 48, 2).x;
}

static void handleInputs(int lane, int action){
	if (settings.mirrorMode && lane!=-1) {
		lane = (diff == 3 ? 4 : 3) - lane;
	}
	if (!streamsLoaded) {
		return;
	}
	Chart& curChart = songList.songs[curPlayingSong].parts[instrument]->charts[diff];
	float eventTime = GetMusicTimePlayed(loadedStreams[0].first);
	if (action == GLFW_PRESS && (lane == -1) && overdriveFill > 0 && !overdrive) {
		overdriveActiveTime = eventTime;
		overdriveActiveFill = overdriveFill;
		overdrive = true;
		overdriveHitAvailable = true;
        overdriveHitTime = eventTime;
	}
	if (lane == -1) {
		Note& curNote = curChart.notes[0];
		for (int i = 0; i < curChart.notes.size(); i++) {
			if ((curChart.notes[i].time) - (goodBackend)+InputOffset < eventTime &&
				(curChart.notes[i].time) + (goodFrontend)+InputOffset > eventTime)
				curNote = curChart.notes[i];
			break;
		}
		if (action == GLFW_PRESS && !overdriveHeld) {
			overdriveHeld = true;
		}
		else if (action == GLFW_RELEASE && overdriveHeld) {
			overdriveHeld = false;
		}
		if (action == GLFW_PRESS && overdriveHitAvailable) {
			if ((curNote.time) - (goodBackend)+InputOffset < eventTime &&
				(curNote.time) + (goodFrontend)+InputOffset > eventTime &&
				!curNote.hit) {
				for (int lane = 0; lane < 5; lane++) {
					int chordLane = curChart.findNoteIdx(curNote.time, lane);
					if (chordLane != -1) {
						Note& chordNote = curChart.notes[chordLane];
						if ((chordNote.time) - (goodBackend)+InputOffset < eventTime && //backend hitwindow
							(chordNote.time) + (goodFrontend)+InputOffset > eventTime && //frontent hitwindow
							!chordNote.hit) { //not hit
							chordNote.hit = true;
							overdriveLanesHit[lane] = true;
							chordNote.hitTime = eventTime;

							if ((chordNote.len) > 0 && !chordNote.lift) {
								chordNote.held = true;
							}
							if ((chordNote.time) - perfectBackend + InputOffset < eventTime && chordNote.time + perfectFrontend + InputOffset > eventTime) {
								chordNote.perfect = true;

							}
							if (chordNote.perfect) lastNotePerfect = true;
							else lastNotePerfect = false;
							player::HitNote(curNote.perfect, instrument);
							curNote.accounted = true;
						}
					}
				}
				overdriveHitAvailable = false;
				overdriveLiftAvailable = true;
			}
		}
		else if (action == GLFW_RELEASE && overdriveLiftAvailable) {
			if ((curNote.time) - (goodBackend * liftTimingMult) + InputOffset < eventTime &&
				(curNote.time) + (goodFrontend * liftTimingMult) + InputOffset > eventTime &&
				!curNote.hit) {
				for (int lane = 0; lane < 5; lane++) {
					if (overdriveLanesHit[lane]) {
						int chordLane = curChart.findNoteIdx(curNote.time, lane);
						if (chordLane != -1) {
							Note& chordNote = curChart.notes[chordLane];
							if (chordNote.lift) {
								chordNote.hit = true;
								overdriveLanesHit[lane] = false;
								chordNote.hitTime = eventTime;

								if ((chordNote.time) - perfectBackend + InputOffset < eventTime && chordNote.time + perfectFrontend + InputOffset > eventTime) {
									chordNote.perfect = true;

								}
								if (chordNote.perfect) lastNotePerfect = true;
								else lastNotePerfect = false;
							}

						}
					}
				}
				overdriveLiftAvailable = false;
			}
		}
		if (action == GLFW_RELEASE && curNote.held && (curNote.len) > 0 && overdriveLiftAvailable) {
			for (int lane = 0; lane < 5; lane++) {
				if (overdriveLanesHit[lane]) {
					int chordLane = curChart.findNoteIdx(curNote.time, lane);
					if (chordLane != -1) {
						Note& chordNote = curChart.notes[chordLane];
						if (chordNote.held && chordNote.len > 0) {
							if (!((diff == 3 && settings.keybinds5K[chordNote.lane]) || (diff != 3 && settings.keybinds4K[chordNote.lane]))) {
								chordNote.held = false;
								score += sustainScoreBuffer[chordNote.lane];
								sustainScoreBuffer[chordNote.lane] = 0;
								mute = true;
							}
						}
					}
				}
			}
		}
	}
	else {
		for (int i = curNoteIdx[lane]; i < curChart.notes_perlane[lane].size(); i++) {
			Note& curNote = curChart.notes[curChart.notes_perlane[lane][i]];

			if (lane != curNote.lane) continue;
			if ((curNote.lift && action == GLFW_RELEASE) || action == GLFW_PRESS) {
				if ((curNote.time) - (action == GLFW_RELEASE ? goodBackend * liftTimingMult : goodBackend) + InputOffset < eventTime &&
					(curNote.time) + ((action == GLFW_RELEASE ? goodFrontend * liftTimingMult : goodFrontend) + InputOffset) > eventTime &&
					!curNote.hit) {
					if (curNote.lift && action == GLFW_RELEASE) {
						lastHitLifts[lane] = curChart.notes_perlane[lane][i];
					}
					curNote.hit = true;
					curNote.hitTime = eventTime;
					if ((curNote.len) > 0 && !curNote.lift) {
						curNote.held = true;
					}
					if ((curNote.time) - perfectBackend + InputOffset < eventTime && curNote.time + perfectFrontend + InputOffset > eventTime) {
						curNote.perfect = true;
					}
					if (curNote.perfect) lastNotePerfect = true;
					else lastNotePerfect = false;
					player::HitNote(curNote.perfect, instrument);
					curNote.accounted = true;
					break;
				}
				if (curNote.miss) lastNotePerfect = false;
			}
			if (action == GLFW_RELEASE && curNote.held && (curNote.len) > 0) {
				curNote.held = false;
				score += sustainScoreBuffer[curNote.lane];
				sustainScoreBuffer[curNote.lane] = 0;
				mute = true;
				// SetAudioStreamVolume(loadedStreams[instrument].stream, missVolume);
			}

			if (action == GLFW_PRESS &&
				eventTime > songList.songs[curPlayingSong].music_start &&
				!curNote.hit &&
				!curNote.accounted &&
				((curNote.time) - perfectBackend) + InputOffset > eventTime &&
				eventTime > overdriveHitTime + 0.05
				&& !overhitFrets[lane]) {
				if (lastHitLifts[lane] != -1) {
					if (eventTime > curChart.notes[lastHitLifts[lane]].time - 0.1 && eventTime < curChart.notes[lastHitLifts[lane]].time + 0.1)
						continue;
				}
				player::OverHit();
				if (curChart.odPhrases.size() > 0 && eventTime >= curChart.odPhrases[curODPhrase].start && eventTime < curChart.odPhrases[curODPhrase].end && !curChart.odPhrases[curODPhrase].missed) curChart.odPhrases[curODPhrase].missed = true;
				overhitFrets[lane] = true;
			}
		}
	}
	
}

// what to check when a key changes states (what was the change? was it pressed? or released? what time? what window? were any modifiers pressed?)
static void keyCallback(GLFWwindow* wind, int key, int scancode, int action, int mods) {
	if (!streamsLoaded) {
		return;
	}
	if (action < 2) {  // if the key action is NOT repeat (release is 0, press is 1)
		int lane = -2;
		if (key == settings.keybindOverdrive || key == settings.keybindOverdriveAlt) {
			handleInputs(-1, action);
		}
		else {
			if (diff == 3) {
				for (int i = 0; i < 5; i++) {
					if (key == settings.keybinds5K[i] && !heldFretsAlt[i]) {
						if (action == GLFW_PRESS) {
							heldFrets[i] = true;
						}
						else if (action == GLFW_RELEASE) {
							heldFrets[i] = false;
							overhitFrets[i] = false;
						}
						lane = i;
					}
					else if (key == settings.keybinds5KAlt[i] && !heldFrets[i]) {
						if (action == GLFW_PRESS) {
							heldFretsAlt[i] = true;
						}
						else if (action == GLFW_RELEASE) {
							heldFretsAlt[i] = false;
							overhitFrets[i] = false;
						}
						lane = i;
					}
				}
			}
			else {
				for (int i = 0; i < 4; i++) {
					if (key == settings.keybinds4K[i] && !heldFretsAlt[i]) {
						if (action == GLFW_PRESS) {
							heldFrets[i] = true;
						}
						else if (action == GLFW_RELEASE) {
							heldFrets[i] = false;
							overhitFrets[i] = false;
						}
						lane = i;
					}
					else if (key == settings.keybinds4KAlt[i] && !heldFrets[i]) {
						if (action == GLFW_PRESS) {
							heldFretsAlt[i] = true;
						}
						else if (action == GLFW_RELEASE) {
							heldFretsAlt[i] = false;
							overhitFrets[i] = false;
						}
						lane = i;
					}
				}
			}
			if (lane > -1) {
				handleInputs(lane, action);
			}
			
		}
	}
}

static void gamepadStateCallback(int jid, GLFWgamepadstate state) {
	if (!streamsLoaded) {
		return;
	}
	double eventTime = GetMusicTimePlayed(loadedStreams[0].first);
	if (settings.controllerOverdrive >= 0) {
		if (state.buttons[settings.controllerOverdrive] != buttonValues[settings.controllerOverdrive]) {
			buttonValues[settings.controllerOverdrive] = state.buttons[settings.controllerOverdrive];
			handleInputs(-1, state.buttons[settings.controllerOverdrive]);
		}
	}
	else {
		if (state.axes[-(settings.controllerOverdrive+1)] != axesValues[-(settings.controllerOverdrive + 1)]) {
			axesValues[-(settings.controllerOverdrive + 1)] = state.axes[-(settings.controllerOverdrive + 1)];
			if (state.axes[-(settings.controllerOverdrive + 1)] == 1.0f*settings.controllerOverdriveAxisDirection) {
				handleInputs(-1, GLFW_PRESS);
			}
			else {
				handleInputs(-1, GLFW_RELEASE);
			}
		}
	}
	if (diff == 3) {
		for (int i = 0; i < 5; i++) {
			if (settings.controller5K[i] >= 0) {
				if (state.buttons[settings.controller5K[i]] != buttonValues[settings.controller5K[i]]) {
					if (state.buttons[settings.controller5K[i]] == 1)
						heldFrets[i] = true;
					else {
						heldFrets[i] = false;
						overhitFrets[i] = false;
					}
						
					handleInputs(i, state.buttons[settings.controller5K[i]]);
					buttonValues[settings.controller5K[i]] = state.buttons[settings.controller5K[i]];
				}
			}
			else {
				if (state.axes[-(settings.controller5K[i] + 1)] != axesValues[-(settings.controller5K[i]+1)]) {
					if (state.axes[-(settings.controller5K[i] + 1)] == 1.0f * (float)settings.controller5KAxisDirection[i]) {
						heldFrets[i] = true;
						handleInputs(i, GLFW_PRESS);
					}
					else {
						heldFrets[i] = false;
						overhitFrets[i] = false;
						handleInputs(i, GLFW_RELEASE);
					}
					axesValues[-(settings.controller5K[i] + 1)] = state.axes[-(settings.controller5K[i] + 1)];
				}
			}
		}
	}
	else {
		for (int i = 0; i < 4; i++) {
			if (settings.controller4K[i] >= 0) {
				if (state.buttons[settings.controller4K[i]] != buttonValues[settings.controller4K[i]]) {
					if (state.buttons[settings.controller4K[i]] == 1)
						heldFrets[i] = true;
					else {
						heldFrets[i] = false;
						overhitFrets[i] = false;
					}
					handleInputs(i, state.buttons[settings.controller4K[i]]);
					buttonValues[settings.controller4K[i]] = state.buttons[settings.controller4K[i]];
				}
			}
			else {
				if (state.axes[-(settings.controller4K[i] + 1)] != axesValues[-(settings.controller4K[i] + 1)]) {
					if (state.axes[-(settings.controller4K[i] + 1)] == 1.0f * (float)settings.controller4KAxisDirection[i]) {
						heldFrets[i] = true;
						handleInputs(i, GLFW_PRESS);
					}
					else {
						heldFrets[i] = false;
						overhitFrets[i] = false;
						handleInputs(i, GLFW_RELEASE);
					}
					axesValues[-(settings.controller4K[i] + 1)] = state.axes[-(settings.controller4K[i] + 1)];
				}
			}
		}
	}
}

static void gamepadStateCallbackSetControls(int jid, GLFWgamepadstate state) {
	for (int i = 0; i < 6; i++) {
		axesValues2[i] = state.axes[i];
	}
	if (changingKey || changingOverdrive) {
		for (int i = 0; i < 15; i++) {
			if (state.buttons[i] == 1) {
				if (buttonValues[i] == 0) {
					controllerID = jid;
					pressedGamepadInput = i;
					return;
				}
				else {
					buttonValues[i] = state.buttons[i];
				}
			}
		}
		for (int i = 0; i < 6; i++) {
			if (state.axes[i] == 1.0f || (i <= 3 && state.axes[i] == -1.0f)) {
				axesValues[i] = 0.0f;
				if (state.axes[i] == 1.0f) axisDirection = 1;
				else axisDirection = -1;
				controllerID = jid;
				pressedGamepadInput = -(1 + i);
				return;
			}
			else {
				axesValues[i] = 0.0f;
			}
		}
	}
	else {
		for (int i = 0; i < 15; i++) {
			buttonValues[i] = state.buttons[i];
		}
		for (int i = 0; i < 6; i++) {
			axesValues[i] = state.axes[i];
		}
		pressedGamepadInput = -999;
	}
}

int minWidth = 640;
int minHeight = 480;


int main(int argc, char* argv[])
{
	SetConfigFlags(FLAG_MSAA_4X_HINT);
	SetConfigFlags(FLAG_WINDOW_RESIZABLE);
	SetConfigFlags(FLAG_VSYNC_HINT);
	
	SetTraceLogLevel(LOG_NONE);

	// 800 , 600
	InitWindow(1, 1, "Encore");
	SetWindowSize(GetMonitorWidth(GetCurrentMonitor()) * 0.75, GetMonitorHeight(GetCurrentMonitor()) * 0.75);
	SetWindowPosition((GetMonitorWidth(GetCurrentMonitor())*0.5)-(GetMonitorWidth(GetCurrentMonitor())*0.375), (GetMonitorHeight(GetCurrentMonitor()) * 0.5) - (GetMonitorHeight(GetCurrentMonitor()) * 0.375));
	
	bool windowToggle = true;
	ArgumentList::InitArguments(argc, argv);

	std::string FPSCapStringVal = ArgumentList::GetArgValue("fpscap");
	int targetFPSArg = 0;

	if (FPSCapStringVal != "")
	{
#ifdef NDEBUG
		str2int(&targetFPSArg, FPSCapStringVal.c_str());
#else		
		assert(str2int(&targetFPSArg, FPSCapStringVal.c_str()) == STR2INT_SUCCESS);
#endif
		TraceLog(LOG_INFO, "Argument overridden target FPS: %d", targetFPSArg);
	}


	//https://www.raylib.com/examples/core/loader.html?name=core_custom_frame_control

	double previousTime = GetTime();
	double currentTime = 0.0;
	double updateDrawTime = 0.0;
	double waitTime = 0.0;
	float deltaTime = 0.0f;




	float timeCounter = 0.0f;

	int targetFPS = targetFPSArg == 0 ? GetMonitorRefreshRate(GetCurrentMonitor()) : targetFPSArg;
	std::vector<std::string> songPartsList{ "Drums","Bass","Guitar","Vocals" };
	std::vector<std::string> diffList{ "Easy","Medium","Hard","Expert" };
	TraceLog(LOG_INFO, "Target FPS: %d", targetFPS);

	InitAudioDevice();

	SetExitKey(0);

	Camera3D camera = { 0 };

	// Y UP!!!! REMEMBER!!!!!!
	//							  x,    y,     z
							// 0.0f, 5.0f, -3.5f
	//								 6.5f
	camera.position = Vector3{ 0.0f, 7.0f, -10.0f };
	// 0.0f, 0.0f, 6.5f
	camera.target = Vector3{ 0.0f, 0.0f, 13.0f };

	camera.up = Vector3{ 0.0f, 1.0f, 0.0f };
	camera.fovy = 34.5f;
	camera.projection = CAMERA_PERSPECTIVE;


	std::filesystem::path executablePath(GetApplicationDirectory());

	std::filesystem::path directory = executablePath.parent_path();

	std::filesystem::path songsPath = directory / "Songs";
	if (std::filesystem::exists(directory / "keybinds.json")) {
		settings.migrateSettings(directory / "keybinds.json", directory / "settings.json");
	}
	settings.loadSettings(directory / "settings.json");
	trackSpeedButton = "Track Speed " + truncateFloatString(settings.trackSpeedOptions[settings.trackSpeed]) + "x";
	songList = LoadSongs(songsPath);

	ChangeDirectory(GetApplicationDirectory());
	assets.loadAssets(directory);
    SetWindowIcon(assets.icon);
	GLFWkeyfun origKeyCallback = glfwSetKeyCallback(glfwGetCurrentContext(), keyCallback);
	GLFWgamepadstatefun origGamepadCallback = glfwSetGamepadStateCallback(gamepadStateCallback);
	glfwSetKeyCallback(glfwGetCurrentContext(), origKeyCallback);
	glfwSetGamepadStateCallback(origGamepadCallback);
	GuiSetStyle(DEFAULT, BACKGROUND_COLOR, 0x505050ff);
	GuiSetStyle(DEFAULT, TEXT_SIZE, 20);
	GuiSetFont(assets.rubik);
	while (!WindowShouldClose())
	{
		double curTime = GetTime();
		if (GetScreenWidth() < minWidth) {
			if (GetScreenHeight() < minHeight)
				SetWindowSize(minWidth, minHeight);
			else
				SetWindowSize(minWidth, GetScreenHeight());
		}
		if (GetScreenHeight() < minHeight) {
			if (GetScreenWidth() < minWidth)
				SetWindowSize(minWidth, minHeight);
			else
				SetWindowSize(GetScreenWidth(), minHeight);
		}
		float diffDistance = diff == 3 ? 2.0f : 1.5f;
		float lineDistance = diff == 3 ? 1.5f : 1.0f;
		BeginDrawing();

		ClearBackground(DARKGRAY);

		switch (currentScreen) {
			case MENU: {

                DrawTextureEx(assets.encoreWhiteLogo, {(float)GetScreenWidth()/2 - assets.encoreWhiteLogo.width/4, ((float)GetScreenHeight()/5 - assets.encoreWhiteLogo.height/4)},0,0.5, WHITE);

                if (GuiButton({ ((float)GetScreenWidth() / 2) - 100,((float)GetScreenHeight() / 2) - 120,200, 60 }, "Play")) {
					for (Song& song : songList.songs) {
						song.titleScrollTime = GetTime();
						song.titleTextWidth = MeasureTextRubik(song.title.c_str(), 30);
						song.artistScrollTime = GetTime();
						song.artistTextWidth = MeasureTextRubik(song.artist.c_str(), 20);
					}
					currentScreen = SONG_SELECT;
				}
				if (GuiButton({ ((float)GetScreenWidth() / 2) - 100,((float)GetScreenHeight() / 2) - 30,200, 60 }, "Options")) {
					glfwSetGamepadStateCallback(gamepadStateCallbackSetControls);
					currentScreen = SETTINGS;
				}
				if (GuiButton({ ((float)GetScreenWidth() / 2) - 100,((float)GetScreenHeight() / 2) + 60,200, 60 }, "Quit")) {
					exit(0);
				}
				break;
			}
			case SETTINGS: {
				if (settings.controllerType == -1 && controllerID != -1) {
					std::string gamepadName = std::string(glfwGetGamepadName(controllerID));
					settings.controllerType = getControllerType(gamepadName);
				}
				if (GuiButton({ ((float)GetScreenWidth() / 2) - 350,((float)GetScreenHeight() - 60),100,60 }, "Cancel") && !(changingKey || changingOverdrive)) {
					glfwSetGamepadStateCallback(origGamepadCallback);
					settings.keybinds4K = settings.prev4K;
					settings.keybinds5K = settings.prev5K;
					settings.keybinds4KAlt = settings.prev4KAlt;
					settings.keybinds5KAlt = settings.prev5KAlt;
					settings.keybindOverdrive = settings.prevOverdrive;
					settings.keybindOverdriveAlt = settings.prevOverdriveAlt;

					settings.controller4K = settings.prevController4K;
					settings.controller4KAxisDirection = settings.prevController4KAxisDirection;
					settings.controller5K = settings.prevController5K;
					settings.controller5KAxisDirection = settings.prevController5KAxisDirection;
					settings.controllerOverdrive = settings.prevControllerOverdrive;
					settings.controllerOverdriveAxisDirection = settings.prevControllerOverdriveAxisDirection;
					settings.controllerType = settings.prevControllerType;

					settings.trackSpeed = settings.prevTrackSpeed;
					settings.inputOffsetMS = settings.prevInputOffsetMS;
					settings.avOffsetMS = settings.prevAvOffsetMS;
					settings.missHighwayDefault = settings.prevMissHighwayColor;
					settings.mirrorMode = settings.prevMirrorMode;

					settings.saveSettings(directory / "settings.json");

					currentScreen = MENU;
				}
				if (GuiButton({ ((float)GetScreenWidth() / 2) + 250,((float)GetScreenHeight() - 60),100,60 }, "Apply") && !(changingKey || changingOverdrive)) {
					glfwSetGamepadStateCallback(origGamepadCallback);
					settings.prev4K = settings.keybinds4K;
					settings.prev5K = settings.keybinds5K;
					settings.prev4KAlt = settings.keybinds4KAlt;
					settings.prev5KAlt = settings.keybinds5KAlt;
					settings.prevOverdrive = settings.keybindOverdrive;
					settings.prevOverdriveAlt = settings.keybindOverdriveAlt;

					settings.prevController4K = settings.controller4K;
					settings.prevController4KAxisDirection = settings.controller4KAxisDirection;
					settings.prevController5K = settings.controller5K;
					settings.prevController5KAxisDirection = settings.controller5KAxisDirection;
					settings.prevControllerOverdrive = settings.controllerOverdrive;
					settings.prevControllerOverdriveAxisDirection = settings.controllerOverdriveAxisDirection;
					settings.prevControllerType = settings.controllerType;

					settings.prevTrackSpeed = settings.trackSpeed;
					settings.prevInputOffsetMS = settings.inputOffsetMS;
					settings.prevAvOffsetMS = settings.avOffsetMS;
					settings.prevMissHighwayColor = settings.missHighwayDefault;
					settings.prevMirrorMode = settings.mirrorMode;

					settings.saveSettings(directory / "settings.json");

					currentScreen = MENU;
				}
				static int selectedTab = 0;
				GuiToggleGroup({ 0,0,(float)GetScreenWidth() / 3,60 }, "Main;Keyboard Controls;Gamepad Controls", &selectedTab);
				if (selectedTab == 0) { //Main settings tab
					if (GuiButton({ (float)GetScreenWidth() / 2 - 125,(float)GetScreenHeight() / 2 - 320,250,60 }, "")) {
						if (settings.trackSpeed == settings.trackSpeedOptions.size() - 1) settings.trackSpeed = 0; else settings.trackSpeed++;
						trackSpeedButton = "Track Speed " + truncateFloatString(settings.trackSpeedOptions[settings.trackSpeed]) + "x";
					}
					DrawTextRubik(trackSpeedButton.c_str(), (float)GetScreenWidth() / 2 - MeasureTextRubik(trackSpeedButton.c_str(), 20) / 2, (float)GetScreenHeight() / 2 - 300, 20, BLACK);
					float avOffsetFloat = (float)settings.avOffsetMS;
					DrawTextRubik("A/V Offset", (float)GetScreenWidth() / 2 - MeasureTextRubik("A/V Offset", 20) / 2, (float)GetScreenHeight() / 2 - 240, 20, WHITE);
					DrawTextRubik(" -500 ", (float)GetScreenWidth() / 2 - 125 - MeasureTextRubik(" -500 ", 20), (float)GetScreenHeight() / 2 - 210, 20, WHITE);
					DrawTextRubik(" 500 ", (float)GetScreenWidth() / 2 + 125, (float)GetScreenHeight() / 2 - 210, 20, WHITE);
					if (GuiSliderBar({ (float)GetScreenWidth() / 2 - 125,(float)GetScreenHeight() / 2 - 220,250,40 }, "", "", &avOffsetFloat, -500.0f, 500.0f)) {
						settings.avOffsetMS = (int)avOffsetFloat;
					}
					if (GuiButton({ (float)GetScreenWidth() / 2 - 125 - MeasureTextRubik(" -500 ", 20) - 60,(float)GetScreenHeight() / 2 - 230,60,60 }, "-1")) {
						settings.avOffsetMS--;
					}
					if (GuiButton({ (float)GetScreenWidth() / 2 + 125 + MeasureTextRubik(" -500 ", 20) ,(float)GetScreenHeight() / 2 - 230,60,60 }, "+1")) {
						settings.avOffsetMS++;
					}
					DrawTextRubik(std::to_string(settings.avOffsetMS).c_str(), (float)GetScreenWidth() / 2 - (MeasureTextRubik(std::to_string(settings.avOffsetMS).c_str(), 20) / 2), (float)GetScreenHeight() / 2 - 210, 20, BLACK);

					if (GuiButton({ (float)GetScreenWidth() / 2 - 125, (float)GetScreenHeight() / 2,250,60 }, TextFormat("Miss Highway Color: %s", MissHighwayColor ? "True" : "False"))) {
						settings.missHighwayDefault = !settings.missHighwayDefault;
						MissHighwayColor = settings.missHighwayDefault;
					};
					if (GuiButton({ (float)GetScreenWidth() / 2 - 125, (float)GetScreenHeight() / 2 + 90,250,60 }, TextFormat("Mirror mode: %s", settings.mirrorMode ? "True" : "False"))) {
						settings.mirrorMode = !settings.mirrorMode;
					};
					float inputOffsetFloat = (float)settings.inputOffsetMS;
					DrawTextRubik("Input Offset", (float)GetScreenWidth() / 2 - MeasureTextRubik("Input Offset", 20) / 2, (float)GetScreenHeight() / 2 - 160, 20, WHITE);
					DrawTextRubik(" -500 ", (float)GetScreenWidth() / 2 - 125 - MeasureTextRubik(" -500 ", 20), (float)GetScreenHeight() / 2 - 130, 20, WHITE);
					DrawTextRubik(" 500 ", (float)GetScreenWidth() / 2 + 125, (float)GetScreenHeight() / 2 - 130, 20, WHITE);
					if (GuiSliderBar({ (float)GetScreenWidth() / 2 - 125,(float)GetScreenHeight() / 2 - 140,250,40 }, "", "", &inputOffsetFloat, -500.0f, 500.0f)) {
						settings.inputOffsetMS = (int)inputOffsetFloat;
					}
					if (GuiButton({ (float)GetScreenWidth() / 2 - 125 - MeasureTextRubik(" -500 ", 20) - 60,(float)GetScreenHeight() / 2 - 150,60,60 }, "-1")) {
						settings.inputOffsetMS--;
					}
					if (GuiButton({ (float)GetScreenWidth() / 2 + 125 + MeasureTextRubik(" -500 ", 20),(float)GetScreenHeight() / 2 - 150,60,60 }, "+1")) {
						settings.inputOffsetMS++;
					}
					DrawTextRubik(std::to_string(settings.inputOffsetMS).c_str(), (float)GetScreenWidth() / 2 - (MeasureTextRubik(std::to_string(settings.inputOffsetMS).c_str(), 20) / 2), (float)GetScreenHeight() / 2 - 130, 20, BLACK);
				}
				else if (selectedTab == 1) { //Keyboard bindings tab
					for (int i = 0; i < 5; i++) {
						float j = i - 2.0f;
						if (GuiButton({ ((float)GetScreenWidth() / 2) - 40 + (80 * j),120,80,60 }, getKeyStr(settings.keybinds5K[i]).c_str())) {
							settings.changing4k = false;
							settings.changingAlt = false;
							selLane = i;
							changingKey = true;
						}
						if (GuiButton({ ((float)GetScreenWidth() / 2) - 40 + (80 * j),180,80,60 }, getKeyStr(settings.keybinds5KAlt[i]).c_str())) {
							settings.changing4k = false;
							settings.changingAlt = true;
							selLane = i;
							changingKey = true;
						}
					}
					for (int i = 0; i < 4; i++) {
						float j = i - 1.5f;
						if (GuiButton({ ((float)GetScreenWidth() / 2) - 40 + (80 * j),300,80,60 }, getKeyStr(settings.keybinds4K[i]).c_str())) {
							settings.changingAlt = false;
							settings.changing4k = true;
							selLane = i;
							changingKey = true;
						}
						if (GuiButton({ ((float)GetScreenWidth() / 2) - 40 + (80 * j),360,80,60 }, getKeyStr(settings.keybinds4KAlt[i]).c_str())) {
							settings.changingAlt = true;
							settings.changing4k = true;
							selLane = i;
							changingKey = true;
						}
					}
					if (GuiButton({ ((float)GetScreenWidth() / 2) - 100,480,80,60 }, getKeyStr(settings.keybindOverdrive).c_str())) {
						settings.changingAlt = false;
						changingKey = false;
						changingOverdrive = true;
					}
					if (GuiButton({ ((float)GetScreenWidth() / 2) + 20,480,80,60 }, getKeyStr(settings.keybindOverdriveAlt).c_str())) {
						settings.changingAlt = true;
						changingKey = false;
						changingOverdrive = true;
					}
					if (changingKey) {
						std::vector<int>& bindsToChange = settings.changingAlt ? (settings.changing4k ? settings.keybinds4KAlt : settings.keybinds5KAlt) : (settings.changing4k ? settings.keybinds4K : settings.keybinds5K);
						DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), { 0,0,0,200 });
						std::string keyString = (settings.changing4k ? "4k" : "5k");
						std::string altString = (settings.changingAlt ? " alt" : "");
						std::string changeString = "Press a key for " + keyString + altString + " lane ";
						DrawTextRubik(changeString.c_str(), (GetScreenWidth() - MeasureTextRubik(changeString.c_str(), 20)) / 2, GetScreenHeight() / 2 - 30, 20, WHITE);
						DrawTextRubik("Or press escape to remove bound key", (GetScreenWidth() - MeasureTextRubik("Or press escape to remove bound key", 20)) / 2, GetScreenHeight() / 2 + 30, 20, WHITE);
						int pressedKey = GetKeyPressed();
						if (pressedKey != 0) {
							if (pressedKey == KEY_ESCAPE)
								pressedKey = -1;
							bindsToChange[selLane] = pressedKey;
							selLane = 0;
							changingKey = false;
						}
					}
					if (changingOverdrive) {
						DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), { 0,0,0,200 });
						std::string altString = (settings.changingAlt ? " alt" : "");
						std::string changeString = "Press a key for " + altString + " overdrive";
						DrawTextRubik(changeString.c_str(), (GetScreenWidth() - MeasureTextRubik(changeString.c_str(), 20)) / 2, GetScreenHeight() / 2 - 30, 20, WHITE);
						DrawTextRubik("Or press escape to remove bound key", (GetScreenWidth() - MeasureTextRubik("Or press escape to remove bound key", 20)) / 2, GetScreenHeight() / 2 + 30, 20, WHITE);
						int pressedKey = GetKeyPressed();
						if (pressedKey != 0) {
							if (pressedKey == KEY_ESCAPE)
								pressedKey = -1;
							if (settings.changingAlt)
								settings.keybindOverdriveAlt = pressedKey;
							else
								settings.keybindOverdrive = pressedKey;
							changingOverdrive = false;
						}
					}
				}
				else if (selectedTab == 2) { //Controller bindings tab
					for (int i = 0; i < 5; i++) {
						float j = i - 2.0f;
						if (GuiButton({ ((float)GetScreenWidth() / 2) - 40 + (80 * j),240,80,60 }, getControllerStr(controllerID, settings.controller5K[i], settings.controllerType, settings.controller5KAxisDirection[i]).c_str())) {
							settings.changing4k = false;
							selLane = i;
							changingKey = true;
						}
					}
					for (int i = 0; i < 4; i++) {
						float j = i - 1.5f;
						if (GuiButton({ ((float)GetScreenWidth() / 2) - 40 + (80 * j),360,80,60 }, getControllerStr(controllerID, settings.controller4K[i], settings.controllerType, settings.controller4KAxisDirection[i]).c_str())) {
							settings.changing4k = true;
							selLane = i;
							changingKey = true;
						}
					}
					if (GuiButton({ ((float)GetScreenWidth() / 2) - 40,480,80,60 }, getControllerStr(controllerID, settings.controllerOverdrive, settings.controllerType, settings.controllerOverdriveAxisDirection).c_str())) {
						changingKey = false;
						changingOverdrive = true;
					}
					if (changingKey) {
						std::vector<int>& bindsToChange = (settings.changing4k ? settings.controller4K : settings.controller5K);
						std::vector<int>& directionToChange = (settings.changing4k ? settings.controller4KAxisDirection : settings.controller5KAxisDirection);
						DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), { 0,0,0,200 });
						std::string keyString = (settings.changing4k ? "4k" : "5k");
						std::string changeString = "Press a button/axis for controller " + keyString + " lane " + std::to_string(selLane + 1);
						DrawTextRubik(changeString.c_str(), (GetScreenWidth() - MeasureTextRubik(changeString.c_str(), 20)) / 2, GetScreenHeight() / 2 - 30, 20, WHITE);
						DrawTextRubik("Or press escape to cancel", (GetScreenWidth() - MeasureTextRubik("Or press escape to cancel", 20)) / 2, GetScreenHeight() / 2 + 30, 20, WHITE);
						if (pressedGamepadInput != -999) {
							bindsToChange[selLane] = pressedGamepadInput;
							if (pressedGamepadInput < 0) {
								directionToChange[selLane] = axisDirection;
							}
							selLane = 0;
							changingKey = false;
							pressedGamepadInput = -999;
						}
					}
					if (changingOverdrive) {
						DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), { 0,0,0,200 });
						std::string changeString = "Press a button/axis for controller overdrive";
						DrawTextRubik(changeString.c_str(), (GetScreenWidth() - MeasureTextRubik(changeString.c_str(), 20)) / 2, GetScreenHeight() / 2 - 30, 20, WHITE);
						DrawTextRubik("Or press escape to cancel", (GetScreenWidth() - MeasureTextRubik("Or press escape to cancel", 20)) / 2, GetScreenHeight() / 2 + 30, 20, WHITE);

						if (pressedGamepadInput != -999) {
							settings.controllerOverdrive = pressedGamepadInput;
							if (pressedGamepadInput < 0) {
								settings.controllerOverdriveAxisDirection = axisDirection;
							}
							changingOverdrive = false;
							pressedGamepadInput = -999;
						}
					}
					if (IsKeyPressed(KEY_ESCAPE)) {
						pressedGamepadInput = -999;
						changingKey = false;
						changingOverdrive = false;
					}
				}
				break;
			}
			case SONG_SELECT: {
				streamsLoaded = false;
				midiLoaded = false;
				isPlaying = false;
				overdrive = false;
				curNoteIdx = { 0,0,0,0,0 };
				curODPhrase = 0;
				curBeatLine = 0;
				curBPM = 0;
				instrument = 0;
				diff = 0;
				if (GuiButton({ 0,0,60,60 }, "<")) {
					for (Song& song : songList.songs) {
						song.titleXOffset = 0;
						song.artistXOffset = 0;
					}
					currentScreen = MENU;
				}
				Vector2 mouseWheel = GetMouseWheelMoveV();
				if (songSelectOffset <= songList.songs.size() + 2 - (GetScreenHeight() / 60) && songSelectOffset >= 0) {
					songSelectOffset -= mouseWheel.y;
				}
				if (songSelectOffset < 0) songSelectOffset = 0;
				if (songSelectOffset > songList.songs.size() + 2 - (GetScreenHeight() / 60)) songSelectOffset = songList.songs.size() + 2 - (GetScreenHeight() / 60);
				if (songSelectOffset >= songList.songs.size()) songSelectOffset = songList.songs.size() - 1;

				for (int i = songSelectOffset; i < songSelectOffset + (GetScreenHeight() / 60) - 2; i++) {
					if (i == songList.songs.size()) {
						break;
					}
					Song& song = songList.songs[i];
					float songYPos = 60 + (60 * (i - songSelectOffset));
					if (GuiButton(Rectangle{ 0, songYPos,((float)GetScreenWidth() * (3.0f / 5.0f)) , 60 }, "")) {
						curPlayingSong = i;
						currentScreen = INSTRUMENT_SELECT;
					}
					DrawTexturePro(song.albumArt, Rectangle{ 0,0,(float)song.albumArt.width,(float)song.albumArt.height }, { 5,songYPos + 5,50,50 }, Vector2{ 0,0 }, 0.0f, RAYWHITE);
					int songTitleWidth = (GetScreenWidth() * (2.0 / 5.0) - 100);
					int songArtistWidth = (GetScreenWidth() * (1.0 / 5.0) - 65);
					if (song.titleTextWidth >= songTitleWidth) {
						if (curTime > song.titleScrollTime && curTime < song.titleScrollTime + 3.0)
							song.titleXOffset = 0;
						if (curTime > song.titleScrollTime + 3.0) {
							song.titleXOffset -= 1;
							if (song.titleXOffset < -(song.titleTextWidth - songTitleWidth)) {
								song.titleXOffset = -(song.titleTextWidth - songTitleWidth);
								song.titleScrollTime = curTime + 3.0;
							}
						}
					}
					BeginScissorMode(65, songYPos + 15, songTitleWidth, 30);
					DrawTextRubik(song.title.c_str(), song.titleXOffset + 65, songYPos + 15, 30, BLACK);
					EndScissorMode();
					if (song.artistTextWidth > songArtistWidth) {
						if (curTime > song.artistScrollTime && curTime < song.artistScrollTime + 3.0)
							song.artistXOffset = 0;
						if (curTime > song.artistScrollTime + 3.0) {
							song.artistXOffset -= 1;
							if (song.artistXOffset < -(song.artistTextWidth - songArtistWidth)) {
								song.artistScrollTime = curTime + 3.0;
							}
						}
					}
					BeginScissorMode(65 + songTitleWidth + 20, songYPos + 20, songArtistWidth, 20);
					DrawTextRubik(song.artist.c_str(), 65 + songTitleWidth + song.artistXOffset + 20, songYPos + 20, 20, BLACK);
					EndScissorMode();
				}
				break;
			}
			case INSTRUMENT_SELECT: {
				if (!midiLoaded) {
					if (!songList.songs[curPlayingSong].midiParsed) {
						smf::MidiFile midiFile;
						midiFile.read(songList.songs[curPlayingSong].midiPath.string());
						songList.songs[curPlayingSong].getTiming(midiFile, 0, midiFile[0]);
						for (int i = 0; i < midiFile.getTrackCount(); i++)
						{
							std::string trackName;
							for (int j = 0; j < midiFile[i].getSize(); j++) {
								if (midiFile[i][j].isMeta()) {
									if ((int)midiFile[i][j][1] == 3) {
										for (int k = 3; k < midiFile[i][j].getSize(); k++) {
											trackName += midiFile[i][j][k];
										}
										SongParts songPart = partFromString(trackName);
										if (trackName == "BEAT")
											songList.songs[curPlayingSong].parseBeatLines(midiFile, i, midiFile[i]);
										else if (trackName == "EVENTS") {
											songList.songs[curPlayingSong].getStartEnd(midiFile, i, midiFile[i]);
										}
										else {
											if (songPart != SongParts::Invalid) {
												for (int diff = 0; diff < 4; diff++) {
													Chart newChart;
													newChart.parseNotes(midiFile, i, midiFile[i], diff, (int)songPart);
													if (newChart.notes.size() > 0) {
														songList.songs[curPlayingSong].parts[(int)songPart]->hasPart = true;
													}
													std::sort(newChart.notes.begin(), newChart.notes.end(), compareNotes);
													int noteIdx = 0;
													for (Note& note : newChart.notes) {
														newChart.notes_perlane[note.lane].push_back(noteIdx);
														noteIdx++;
													}
													songList.songs[curPlayingSong].parts[(int)songPart]->charts.push_back(newChart);
												}
											}
										}
									}
								}
							}
						}
						songList.songs[curPlayingSong].midiParsed = true;
					}
					midiLoaded = true;
				}
				else {
					if (GuiButton({ 0,0,60,60 }, "<")) {
						midiLoaded = false;
						currentScreen = SONG_SELECT;
					}
					for (int i = 0; i < 4; i++) {
						if (songList.songs[curPlayingSong].parts[i]->hasPart) {
							if (GuiButton({ 0,60 + (60 * (float)i),300,60 }, "")) {
								instrument = i;
								int isBassOrVocal = 0;
								if (instrument == 1 || instrument == 3) {
									isBassOrVocal = 1;
								}
								SetShaderValue(assets.odMultShader, assets.isBassOrVocalLoc, &isBassOrVocal, SHADER_UNIFORM_INT);
								currentScreen = DIFFICULTY_SELECT;
							}
							DrawTextRubik(songPartsList[i].c_str(), 20, 75 + (60 * (float)i), 30, BLACK);
							DrawTextRubik((std::to_string(songList.songs[curPlayingSong].parts[i]->diff + 1) + "/7").c_str(), 220, 75 + (60 * (float)i), 30, BLACK);
						}
					}
				}
				break;
			}
			case DIFFICULTY_SELECT: {
				for (int i = 0; i < 4; i++) {
					if (GuiButton({ 0,0,60,60 }, "<")) {
						currentScreen = SONG_SELECT;
					}
					if (songList.songs[curPlayingSong].parts[instrument]->charts[i].notes.size() > 0) {
						if (GuiButton({ 0,60 + (60 * (float)i),300,60 }, "")) {
							diff = i;
							currentScreen = GAMEPLAY;
							isPlaying = true;
							startedPlayingSong = GetTime();
							glfwSetKeyCallback(glfwGetCurrentContext(), keyCallback);
							glfwSetGamepadStateCallback(gamepadStateCallback);
						}
						DrawTextRubik(diffList[i].c_str(), 150 - (MeasureTextRubik(diffList[i].c_str(), 30) / 2), 75 + (60 * (float)i), 30, BLACK);
					}
				}
				break;
			}
			case GAMEPLAY: {
                // IMAGE BACKGROUNDS??????
                ClearBackground(BLACK);
                DrawTextureEx(assets.songBackground, {0,0},0, (float)GetScreenHeight()/assets.songBackground.height,WHITE);
				int starsval = stars(songList.songs[curPlayingSong].parts[instrument]->charts[diff].baseScore);
                for (int i = 0; i < 5; i++) {
                    DrawTextureEx(assets.emptyStar, {5+((float)i*40),(float)GetScreenHeight() - 470},0,0.15f,WHITE);
                }
                for (int i = 0; i < starsval; i++) {
                    DrawTextureEx(goldStars? assets.goldStar : assets.star, {5+((float)i*40),(float)GetScreenHeight() - 470},0,0.15f,WHITE);
                }
				// DrawTextRubik(TextFormat("%s", starsDisplay), 5, GetScreenHeight() - 470, 48, goldStars ? GOLD : WHITE);
				DrawTextRHDI(TextFormat("%01i", score + sustainScoreBuffer[0] + sustainScoreBuffer[1] + sustainScoreBuffer[2] + sustainScoreBuffer[3] + sustainScoreBuffer[4]), 5, GetScreenHeight() - 420,  Color{107, 161, 222,255});
				DrawTextRubik(TextFormat("%01i", combo), 5, GetScreenHeight() - 330, 30, FC ? GOLD : (combo <= 3) ? RED : WHITE);
				DrawTextRubik(TextFormat("%s", FC ? "FC" : ""), 5, GetScreenHeight() - 40, 36, GOLD);
				DrawTextRubik(TextFormat("%s", lastNotePerfect ? "Perfect" : ""), 5, (GetScreenHeight() - 370), 30, GOLD);

				float multFill = (!overdrive ? (float)(multiplier(instrument) - 1) : ((float)(multiplier(instrument) / 2) - 1)) / (float)maxMultForMeter(instrument);
				SetShaderValue(assets.odMultShader, assets.multLoc, &multFill, SHADER_UNIFORM_FLOAT);
				SetShaderValue(assets.multNumberShader, assets.uvOffsetXLoc, &uvOffsetX, SHADER_UNIFORM_FLOAT);
				SetShaderValue(assets.multNumberShader, assets.uvOffsetYLoc, &uvOffsetY, SHADER_UNIFORM_FLOAT);
				float comboFill = comboFillCalc(instrument);
				SetShaderValue(assets.odMultShader, assets.comboCounterLoc, &comboFill, SHADER_UNIFORM_FLOAT);
				SetShaderValue(assets.odMultShader, assets.odLoc, &overdriveFill, SHADER_UNIFORM_FLOAT);
				for (int i = 0; i < 5; i++) {
					DrawTextRubik(TextFormat("Lane %01i index: %01i", i+1, curNoteIdx[i]), 5, (GetScreenHeight() - 280)+(30*i), 24, WHITE);
				}
				if (extraGameplayStats) {
					DrawTextRubik(TextFormat("Perfect Hit: %01i", perfectHit), 5, GetScreenHeight() - 280, 24,
						(perfectHit > 0) ? GOLD : WHITE);
					DrawTextRubik(TextFormat("Max Combo: %01i", maxCombo), 5, GetScreenHeight() - 130, 24, WHITE);
					DrawTextRubik(TextFormat("Multiplier: %01i", multiplier(instrument)), 5, GetScreenHeight() - 100, 24,
						(multiplier(instrument) >= 4) ? SKYBLUE : WHITE);
					DrawTextRubik(TextFormat("Notes Hit: %01i", notesHit), 5, GetScreenHeight() - 250, 24, FC ? GOLD : WHITE);
					DrawTextRubik(TextFormat("Notes Missed: %01i", notesMissed), 5, GetScreenHeight() - 220, 24,
						((combo == 0) && (!FC)) ? RED : WHITE);
					DrawTextRubik(TextFormat("Strikes: %01i", playerOverhits), 5, GetScreenHeight() - 40, 24, FC ? GOLD : WHITE);
				}




                if (GuiButton({ 0,0,60,60 }, "<")) {
					for (Note& note : songList.songs[curPlayingSong].parts[instrument]->charts[diff].notes) {
						note.accounted = false;
						note.hit = false;
						note.miss = false;
						note.held = false;
						note.heldTime = 0;
						note.hitTime = 0;
						note.perfect = false;
						note.countedForODPhrase = false;
					}
					glfwSetKeyCallback(glfwGetCurrentContext(), origKeyCallback);
					glfwSetGamepadStateCallback(origGamepadCallback);
					// notes = songList.songs[curPlayingSong].parts[instrument]->charts[diff].notes.size();
					// notes = songList.songs[curPlayingSong].parts[instrument]->charts[diff];
					for (odPhrase& phrase : songList.songs[curPlayingSong].parts[instrument]->charts[diff].odPhrases) {
						phrase.missed = false;
						phrase.notesHit = 0;
						phrase.added = false;
					}
					currentScreen = SONG_SELECT;
					overdrive = false;
					overdriveFill = 0.0f;
					overdriveActiveFill = 0.0f;
					overdriveActiveTime = 0.0;
					curODPhrase = 0;
					assets.expertHighway.materials[0].maps[MATERIAL_MAP_ALBEDO].texture = assets.highwayTexture;
					assets.emhHighway.materials[0].maps[MATERIAL_MAP_ALBEDO].texture = assets.highwayTexture;
					assets.multBar.materials[0].maps[MATERIAL_MAP_EMISSION].texture = assets.odMultFill;
					assets.multCtr3.materials[0].maps[MATERIAL_MAP_EMISSION].texture = assets.odMultFill;
					assets.multCtr5.materials[0].maps[MATERIAL_MAP_EMISSION].texture = assets.odMultFill;
					assets.expertHighway.materials[0].maps[MATERIAL_MAP_ALBEDO].color = player.accentColor;
					isPlaying = false;
					midiLoaded = false;
					streamsLoaded = false;
                }
                if (!streamsLoaded) {
                    loadedStreams = LoadStems(songList.songs[curPlayingSong].stemsPath);
                    for (auto& stream : loadedStreams) {
                        std::cout << GetMusicTimeLength(stream.first) << std::endl;
                    }

                    streamsLoaded = true;
                    player::resetPlayerStats();
                }
                else {
					if (GetTime() >= GetMusicTimeLength(loadedStreams[0].first) + startedPlayingSong) {
						for (Note& note : songList.songs[curPlayingSong].parts[instrument]->charts[diff].notes) {
							note.accounted = false;
							note.hit = false;
							note.miss = false;
							note.held = false;
							note.heldTime = 0;
							note.hitTime = 0;
							note.perfect = false;
							note.countedForODPhrase = false;
							// notes += 1;
						}
						glfwSetKeyCallback(glfwGetCurrentContext(), origKeyCallback);
						glfwSetGamepadStateCallback(origGamepadCallback);
						// notes = (int)songList.songs[curPlayingSong].parts[instrument]->charts[diff].notes.size();
						for (odPhrase& phrase : songList.songs[curPlayingSong].parts[instrument]->charts[diff].odPhrases) {
							phrase.missed = false;
							phrase.notesHit = 0;
							phrase.added = false;
						}
						overdrive = false;
						overdriveFill = 0.0f;
						overdriveActiveFill = 0.0f;
						overdriveActiveTime = 0.0;
						isPlaying = false;
						midiLoaded = false;
						streamsLoaded = false;
						curODPhrase = 0;
						assets.expertHighway.materials[0].maps[MATERIAL_MAP_ALBEDO].texture = assets.highwayTexture;
						assets.emhHighway.materials[0].maps[MATERIAL_MAP_ALBEDO].texture = assets.highwayTexture;
						assets.multBar.materials[0].maps[MATERIAL_MAP_EMISSION].texture = assets.odMultFill;
						assets.multCtr3.materials[0].maps[MATERIAL_MAP_EMISSION].texture = assets.odMultFill;
						assets.multCtr5.materials[0].maps[MATERIAL_MAP_EMISSION].texture = assets.odMultFill;
						assets.expertHighway.materials[0].maps[MATERIAL_MAP_ALBEDO].color = player.accentColor;
						currentScreen = RESULTS;

					}
					for (auto& stream : loadedStreams) {
						stream.first.looping = false;
						UpdateMusicStream(stream.first);
						PlayMusicStream(stream.first);
						if (instrument == stream.second)
							SetAudioStreamVolume(stream.first.stream, mute ? missVolume : selInstVolume);
						else
							SetAudioStreamVolume(stream.first.stream, otherInstVolume);

					}
				}

				double musicTime = GetMusicTimePlayed(loadedStreams[0].first);
				if (overdrive) {
					assets.expertHighway.materials[0].maps[MATERIAL_MAP_ALBEDO].texture = assets.highwayTextureOD;
					assets.emhHighway.materials[0].maps[MATERIAL_MAP_ALBEDO].texture = assets.highwayTextureOD;
					assets.multBar.materials[0].maps[MATERIAL_MAP_EMISSION].texture = assets.odMultFillActive;
					assets.multCtr3.materials[0].maps[MATERIAL_MAP_EMISSION].texture = assets.odMultFillActive;
					assets.multCtr5.materials[0].maps[MATERIAL_MAP_EMISSION].texture = assets.odMultFillActive;


					overdriveFill = overdriveActiveFill - ((musicTime - overdriveActiveTime) / (1920 / songList.songs[curPlayingSong].bpms[curBPM].bpm));
					if (overdriveFill <= 0) {
						overdrive = false;
						overdriveActiveFill = 0;
						overdriveActiveTime = 0.0;
						assets.expertHighway.materials[0].maps[MATERIAL_MAP_ALBEDO].texture = assets.highwayTexture;
						assets.emhHighway.materials[0].maps[MATERIAL_MAP_ALBEDO].texture = assets.highwayTexture;
						assets.multBar.materials[0].maps[MATERIAL_MAP_EMISSION].texture = assets.odMultFill;
						assets.multCtr3.materials[0].maps[MATERIAL_MAP_EMISSION].texture = assets.odMultFill;
						assets.multCtr5.materials[0].maps[MATERIAL_MAP_EMISSION].texture = assets.odMultFill;

					}
				}
				for (int i = curBPM; i < songList.songs[curPlayingSong].bpms.size(); i++) {
					if (musicTime > songList.songs[curPlayingSong].bpms[i].time && i < songList.songs[curPlayingSong].bpms.size() - 1)
						curBPM++;
				}

				if (musicTime < 5.0) {
					DrawTextRubik(songList.songs[curPlayingSong].title.c_str(), 5, 65, 30, WHITE);
					DrawTextRubik(songList.songs[curPlayingSong].artist.c_str(), 5, 100, 24, WHITE);
				}

				BeginMode3D(camera);
				if (diff == 3) {
					DrawModel(assets.expertHighway, Vector3{ 0,0,0 }, 1.0f, WHITE);
                    DrawModel(assets.expertHighwaySides, Vector3{ 0,0,0 }, 1.0f, WHITE);
					for (int i = 0; i < 5; i++) {
						if (heldFrets[i] || heldFretsAlt[i]) {
							DrawModel(assets.smasherPressed, Vector3{ diffDistance - (float)(i), 0.01f, smasherPos }, 1.0f, WHITE);
						}
						else {
							DrawModel(assets.smasherReg, Vector3{ diffDistance - (float)(i), 0.01f, smasherPos }, 1.0f, WHITE);
						}
					}
                    //DrawModel(assets.lanes, Vector3 {0,0.1f,0}, 1.0f, WHITE);
					for (int i = 0; i < 4; i++) {
						float radius = (i == (settings.mirrorMode ? 2 : 1)) ? 0.05 : 0.02;

						DrawCylinderEx(Vector3{ lineDistance - i, 0, smasherPos + 0.5f }, Vector3{ lineDistance - i, 0, 20 }, radius, radius, 15, Color{ 128,128,128,128 });
					}

					DrawModel(assets.smasherBoard, Vector3{ 0, 0.001f, 0 }, 1.0f, WHITE);
				}
				else {
					DrawModel(assets.emhHighway, Vector3{ 0, 0, 0 }, 1.0f, WHITE);
					for (int i = 0; i < 4; i++) {
						if (heldFrets[i] || heldFretsAlt[i]) {
							DrawModel(assets.smasherPressed, Vector3{ diffDistance - (float)(i), 0, smasherPos }, 1.0f, WHITE);
						}
						else {
							DrawModel(assets.smasherReg, Vector3{ diffDistance - (float)(i), 0, smasherPos }, 1.0f, WHITE);

						}
					}
					for (int i = 0; i < 3; i++) {
						float radius = (i == 1) ? 0.03 : 0.01;
						DrawCylinderEx(Vector3{ lineDistance - (float)i, 0, smasherPos }, Vector3{ lineDistance - (float)i, 0, highwayLength }, radius,
							radius, 4.0f, Color{ 128, 128, 128, 128 });
					}
					DrawModel(assets.smasherBoard, Vector3{ 0, 0.001f, 3 }, 1.0f, WHITE);
				}
				if (songList.songs[curPlayingSong].beatLines.size() >= 0) {
					for (int i = curBeatLine; i < songList.songs[curPlayingSong].beatLines.size(); i++) {
						if (songList.songs[curPlayingSong].beatLines[i].first >= songList.songs[curPlayingSong].music_start && songList.songs[curPlayingSong].beatLines[i].first <= songList.songs[curPlayingSong].end) {
							double relTime = ((songList.songs[curPlayingSong].beatLines[i].first - musicTime) + VideoOffset) * settings.trackSpeedOptions[settings.trackSpeed];
							if (relTime > 1.5) break;
							float radius = songList.songs[curPlayingSong].beatLines[i].second ? 0.03f : 0.0075f;
							DrawCylinderEx(Vector3{ -diffDistance - 0.5f,0,smasherPos + (highwayLength * (float)relTime) }, Vector3{ diffDistance + 0.5f,0,smasherPos + (highwayLength * (float)relTime) }, radius, radius, 4, Color{ 128,128,128,128 });
							if (relTime < -1 && curBeatLine < songList.songs[curPlayingSong].beatLines.size() - 1) {
								curBeatLine++;

							}
						}
					}
				}
				// DrawTriangle3D(Vector3{ 2.5f,0.0f,0.0f }, Vector3{ -2.5f,0.0f,0.0f }, Vector3{ -2.5f,0.0f,20.0f }, BLACK);
				// DrawTriangle3D(Vector3{ 2.5f,0.0f,0.0f }, Vector3{ -2.5f,0.0f,20.0f }, Vector3{ 2.5f,0.0f,20.0f }, BLACK);

				notes = songList.songs[curPlayingSong].parts[instrument]->charts[diff].notes.size();
				DrawModel(assets.odFrame, Vector3{ 0,1.0f,-0.3f }, 0.8f, WHITE);
				DrawModel(assets.odBar, Vector3{ 0,1.0f,-0.3f }, 0.8f, WHITE);
				DrawModel(assets.multFrame, Vector3{ 0,1.0f,-0.3f }, 0.8f, WHITE);
				DrawModel(assets.multBar, Vector3{ 0,1.0f,-0.3f }, 0.8f, WHITE);
				if (instrument == 1 || instrument == 3) {

					DrawModel(assets.multCtr5, Vector3{ 0,1.0f,-0.3f }, 0.8f, WHITE);
				}
				else {

					DrawModel(assets.multCtr3, Vector3{ 0,1.0f,-0.3f }, 0.8f, WHITE);
				}
				DrawModel(assets.multNumber, Vector3{ 0,1.0f,-0.3f }, 0.8f, WHITE);


				// DrawLine3D(Vector3{ 2.5f, 0.05f, 2.0f }, Vector3{ -2.5f, 0.05f, 2.0f}, WHITE);
				Chart& curChart = songList.songs[curPlayingSong].parts[instrument]->charts[diff];
				for (int lane = 0; lane < (diff == 3 ? 5 : 4); lane++) {
					for (int i = curNoteIdx[lane]; i < curChart.notes_perlane[lane].size(); i++) {
						Note& curNote = curChart.notes[curChart.notes_perlane[lane][i]];

						if (curChart.odPhrases.size() > 0) {
							if (curNote.time >= curChart.odPhrases[curODPhrase].start && curNote.time <= curChart.odPhrases[curODPhrase].end && !curChart.odPhrases[curODPhrase].missed) {
								if(curNote.hit) {
									if (curNote.hit && !curNote.countedForODPhrase) {
										curChart.odPhrases[curODPhrase].notesHit++;
										curNote.countedForODPhrase = true;
									}
								}
								curNote.renderAsOD = true;
							}
							if (curChart.odPhrases[curODPhrase].missed) {
								curNote.renderAsOD = false;
							}
							if (curChart.odPhrases[curODPhrase].notesHit == curChart.odPhrases[curODPhrase].noteCount && !curChart.odPhrases[curODPhrase].added && overdriveFill < 1.0f) {
								overdriveFill += 0.25f;
								if (overdriveFill > 1.0f) overdriveFill = 1.0f;
								if (overdrive) {
									overdriveActiveFill = overdriveFill;
									overdriveActiveTime = musicTime;
								}
								curChart.odPhrases[curODPhrase].added = true;
							}
						}
						if (!curNote.hit && !curNote.accounted && curNote.time + 0.1 < musicTime) {
							curNote.miss = true;
							player::MissNote();
							if (curChart.odPhrases.size()>0 && !curChart.odPhrases[curODPhrase].missed && curNote.time>=curChart.odPhrases[curODPhrase].start && curNote.time<curChart.odPhrases[curODPhrase].end) curChart.odPhrases[curODPhrase].missed = true;
							curNote.accounted = true;
						}

						double relTime = ((curNote.time - musicTime) + VideoOffset) * settings.trackSpeedOptions[settings.trackSpeed];
						double relEnd = (((curNote.time + curNote.len) - musicTime) + VideoOffset) * settings.trackSpeedOptions[settings.trackSpeed];
						float notePosX = diffDistance - (1.0f * (settings.mirrorMode ? (diff == 3 ? 4 : 3) - curNote.lane : curNote.lane));
						if (relTime > 1.5) {
							break;
						}
						if (relEnd > 1.5) relEnd = 1.5;
						if (curNote.lift && !curNote.hit) {
							// lifts						//  distance between notes 
							//									(furthest left - lane distance)
							if (curNote.renderAsOD)					//  1.6f	0.8
								DrawModel(assets.liftModelOD, Vector3{ notePosX,0,smasherPos + (highwayLength * (float)relTime) }, 1.1f, WHITE);
							// energy phrase
							else
								DrawModel(assets.liftModel, Vector3{ notePosX,0,smasherPos + (highwayLength * (float)relTime) }, 1.1f, WHITE);
							// regular 
						}
						else {
							// sustains
							if ((curNote.len) > 0) {
								if (curNote.hit && curNote.held) {
									if (curNote.heldTime < (curNote.len * settings.trackSpeedOptions[settings.trackSpeed])) {
										curNote.heldTime = 0.0 - relTime;
										sustainScoreBuffer[curNote.lane] = (curNote.heldTime / curNote.len) * (12 * curNote.beatsLen) * multiplier(instrument);
										if (relTime < 0.0) relTime = 0.0;
									}
									if (relEnd <= 0.0) {
										if (relTime < 0.0) relTime = relEnd;
										score += sustainScoreBuffer[curNote.lane];
										sustainScoreBuffer[curNote.lane] = 0;
										curNote.held = false;
									}
								}
								else if (curNote.hit && !curNote.held) {
									relTime = relTime + curNote.heldTime;
								}

								/*Color SustainColor = Color{ 69,69,69,255 };
								if (curNote.held) {
									if (od) {
										Color SustainColor = Color{ 217, 183, 82 ,255 };
									}
									Color SustainColor = Color{ 172,82,217,255 };
								}*/

								if (curNote.held && !curNote.renderAsOD) {
									DrawCylinderEx(Vector3{ notePosX, 0.05f, smasherPos + (highwayLength * (float)relTime) }, Vector3{ notePosX,0.05f, smasherPos + (highwayLength * (float)relEnd) }, 0.1f, 0.1f, 15, Color{ 230,100,230,255 });
								}
								if (curNote.renderAsOD && curNote.held) {
									DrawCylinderEx(Vector3{ notePosX, 0.05f, smasherPos + (highwayLength * (float)relTime) }, Vector3{ notePosX,0.05f, smasherPos + (highwayLength * (float)relEnd) }, 0.1f, 0.1f, 15, Color{ 255, 255, 180 ,255 });
								}
								if (!curNote.held && curNote.hit || curNote.miss) {
									DrawCylinderEx(Vector3{ notePosX, 0.05f, smasherPos + (highwayLength * (float)relTime) }, Vector3{ notePosX,0.05f, smasherPos + (highwayLength * (float)relEnd) }, 0.1f, 0.1f, 15, Color{ 69,69,69,255 });
								}
								if (!curNote.hit && !curNote.accounted && !curNote.miss) {
									if (curNote.renderAsOD) {
										DrawCylinderEx(Vector3{ notePosX, 0.05f, smasherPos + (highwayLength * (float)relTime) }, Vector3{ notePosX,0.05f, smasherPos + (highwayLength * (float)relEnd) }, 0.1f, 0.1f, 15, Color{ 200, 170, 70 ,255 });
									}
									else {
										DrawCylinderEx(Vector3{ notePosX, 0.05f,
															   smasherPos + (highwayLength * (float)relTime) },
											Vector3{ notePosX, 0.05f,
													smasherPos + (highwayLength * (float)relEnd) }, 0.1f, 0.1f, 15,
											Color{ 150, 19, 150, 255 });
									}
								}


								// DrawLine3D(Vector3{ diffDistance - (1.0f * curNote.lane),0.05f,smasherPos + (12.5f * (float)relTime) }, Vector3{ diffDistance - (1.0f * curNote.lane),0.05f,smasherPos + (12.5f * (float)relEnd) }, Color{ 172,82,217,255 });
							}
							// regular notes
							if (((curNote.len) > 0 && (curNote.held || !curNote.hit)) || ((curNote.len) == 0 && !curNote.hit)) {
								if (curNote.renderAsOD) {
									if ((!curNote.held && !curNote.miss) || !curNote.hit) {
										DrawModel(assets.noteModelOD, Vector3{ notePosX,0,smasherPos + (highwayLength * (float)relTime) }, 1.1f, WHITE);
									};

								}
								else {
									if ((!curNote.held && !curNote.miss) || !curNote.hit) {
										DrawModel(assets.noteModel, Vector3{ notePosX,0,smasherPos + (highwayLength * (float)relTime) }, 1.1f, WHITE);
									};

								}

							}
							assets.expertHighway.materials[0].maps[MATERIAL_MAP_ALBEDO].color = player.accentColor;
						}
						if (curNote.miss) {
							DrawModel(curNote.lift ? assets.liftModel : assets.noteModel, Vector3{ notePosX,0,smasherPos + (highwayLength * (float)relTime) }, 1.0f, RED);
							if (GetMusicTimePlayed(loadedStreams[0].first) < curNote.time + 0.4 && MissHighwayColor) {
								assets.expertHighway.materials[0].maps[MATERIAL_MAP_ALBEDO].color = RED;
							}
							else {
								assets.expertHighway.materials[0].maps[MATERIAL_MAP_ALBEDO].color = player.accentColor;
							}
						}
						if (curNote.hit && GetMusicTimePlayed(loadedStreams[0].first) < curNote.hitTime + 0.15f) {
							DrawCube(Vector3{ notePosX, 0, smasherPos }, 1.0f, 0.5f, 0.5f, curNote.perfect ? Color{ 255,215,0,64 } : Color{ 255,255,255,64 });
							if (curNote.perfect) {
								DrawCube(Vector3{ 3.25, 0, smasherPos }, 1.0f, 0.01f, 0.5f, ORANGE);

							}
						}



						if (relEnd < -1 && curNoteIdx[lane] < curChart.notes_perlane[lane].size() - 1) curNoteIdx[lane] = i + 1;

					}
				}
				if (curChart.odPhrases.size() > 0 && curODPhrase<curChart.odPhrases.size() - 1 && musicTime>curChart.odPhrases[curODPhrase].end && (curChart.odPhrases[curODPhrase].added ||curChart.odPhrases[curODPhrase].missed)) {
					curODPhrase++;
				}
	#ifndef NDEBUG
				// DrawTriangle3D(Vector3{ -diffDistance - 0.5f,0.05f,smasherPos + (highwayLength * goodFrontend * trackSpeed[speedSelection]) }, Vector3{ diffDistance + 0.5f,0.05f,smasherPos + (highwayLength * goodFrontend * trackSpeed[speedSelection]) }, Vector3{ diffDistance + 0.5f,0.05f,smasherPos - (highwayLength * goodBackend * trackSpeed[speedSelection]) }, Color{ 0,255,0,80 });
				// DrawTriangle3D(Vector3{ diffDistance + 0.5f,0.05f,smasherPos - (highwayLength * goodBackend * trackSpeed[speedSelection]) }, Vector3{ -diffDistance - 0.5f,0.05f,smasherPos - (highwayLength * goodBackend * trackSpeed[speedSelection]) }, Vector3{ -diffDistance - 0.5f,0.05f,smasherPos + (highwayLength * goodFrontend * trackSpeed[speedSelection]) }, Color{ 0,255,0,80 });

				// DrawTriangle3D(Vector3{ -diffDistance - 0.5f,0.05f,smasherPos + (highwayLength * perfectFrontend * trackSpeed[speedSelection]) }, Vector3{ diffDistance + 0.5f,0.05f,smasherPos + (highwayLength * perfectFrontend * trackSpeed[speedSelection]) }, Vector3{ diffDistance + 0.5f,0.05f,smasherPos - (highwayLength * perfectBackend * trackSpeed[speedSelection]) }, Color{ 190,255,0,80 });
				// DrawTriangle3D(Vector3{ diffDistance + 0.5f,0.05f,smasherPos - (highwayLength * perfectBackend * trackSpeed[speedSelection]) }, Vector3{ -diffDistance - 0.5f,0.05f,smasherPos - (highwayLength * perfectBackend * trackSpeed[speedSelection]) }, Vector3{ -diffDistance - 0.5f,0.05f,smasherPos + (highwayLength * perfectFrontend * trackSpeed[speedSelection]) }, Color{ 190,255,0,80 });
	#endif
				EndMode3D();
				break;
			}
			case RESULTS: {
				int starsval = stars(songList.songs[curPlayingSong].parts[instrument]->charts[diff].baseScore);
                for (int i = 0; i < starsval; i++) {
                    DrawTextureEx(goldStars? assets.goldStar : assets.star, {((float)GetScreenWidth()/2)+(i*40)-100,84},0,0.15f,WHITE);
                }
/*
				char* starsDisplay = (char*)"";
				if (starsval == 5) {
					starsDisplay = (char*)"*****";
				}
				else if (starsval == 4) {
					starsDisplay = (char*)"****";
				}
				else if (starsval == 3) {
					starsDisplay = (char*)"***";
				}
				else if (starsval == 2) {
					starsDisplay = (char*)"**";
				}
				else if (starsval == 1) {
					starsDisplay = (char*)"*";;
				}
				else {
					starsDisplay = (char*)"";
				}*/
				DrawTextRubik("Results", GetScreenWidth() / 2 - (MeasureTextRubik("Results", 36) / 2), 0, 36, WHITE);
				DrawTextRubik((songList.songs[curPlayingSong].artist + " - " + songList.songs[curPlayingSong].title).c_str(), GetScreenWidth() / 2 - (MeasureTextRubik((songList.songs[curPlayingSong].artist + " - " + songList.songs[curPlayingSong].title).c_str(), 24) / 2), 48, 24, WHITE);
				if (FC) {
					DrawTextRubik("Flawless!", GetScreenWidth() / 2 - (MeasureTextRubik("Flawless!", 24) / 2), 84, 24, GOLD);
				}
				DrawTextRHDI(TextFormat("%01i", score), (GetScreenWidth() / 2) - MeasureTextRHDI(TextFormat("%01i", score))/2, 160, Color{107, 161, 222,255});
				// DrawTextRubik(TextFormat("%s", starsDisplay), (GetScreenWidth() / 2 - MeasureTextRubik(TextFormat("%s", starsDisplay), 24) / 2), 160, 24, goldStars ? GOLD : WHITE);
				DrawTextRubik(TextFormat("Perfect Notes : %01i/%02i", perfectHit, songList.songs[curPlayingSong].parts[instrument]->charts[diff].notes.size()), (GetScreenWidth() / 2 - MeasureTextRubik(TextFormat("Perfect Notes: %01i/%02i", perfectHit, songList.songs[curPlayingSong].parts[instrument]->charts[diff].notes.size()), 24) / 2), 192, 24, WHITE);
				DrawTextRubik(TextFormat("Good Notes : %01i/%02i", notesHit - perfectHit, songList.songs[curPlayingSong].parts[instrument]->charts[diff].notes.size()), (GetScreenWidth() / 2 - MeasureTextRubik(TextFormat("Good Notes: %01i/%02i", notesHit - perfectHit, songList.songs[curPlayingSong].parts[instrument]->charts[diff].notes.size()), 24) / 2), 224, 24, WHITE);
				DrawTextRubik(TextFormat("Missed Notes: %01i/%02i", notesMissed, songList.songs[curPlayingSong].parts[instrument]->charts[diff].notes.size()), (GetScreenWidth() / 2 - MeasureTextRubik(TextFormat("Missed Notes: %01i/%02i", notesMissed, songList.songs[curPlayingSong].parts[instrument]->charts[diff].notes.size()), 24) / 2), 256, 24, WHITE);
				DrawTextRubik(TextFormat("Strikes: %01i", playerOverhits), (GetScreenWidth() / 2 - MeasureTextRubik(TextFormat("Strikes: %01i", playerOverhits), 24) / 2), 288, 24, WHITE);
				DrawTextRubik(TextFormat("Longest Streak: %01i", maxCombo), (GetScreenWidth() / 2 - MeasureTextRubik(TextFormat("Longest Streak: %01i", maxCombo), 24) / 2), 320, 24, WHITE);
				if (GuiButton({ 0,0,60,60 }, "<")) {
					currentScreen = SONG_SELECT;
				}
				break;
			}
		}
		EndDrawing();

		//SwapScreenBuffer();

		currentTime = GetTime();
		updateDrawTime = currentTime - previousTime;

		if (targetFPS > 0)
		{
			waitTime = (1.0f / (float)targetFPS) - updateDrawTime;
			if (waitTime > 0.0)
			{
				WaitTime((float)waitTime);
				currentTime = GetTime();
				deltaTime = (float)(currentTime - previousTime);
			}
		}
		else deltaTime = (float)updateDrawTime;

		previousTime = currentTime;
	}
	CloseWindow();
	return 0;
}
