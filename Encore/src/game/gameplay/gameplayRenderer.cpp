//
// Created by marie on 09/06/2024.
//

#include "game/gameplay/gameplayRenderer.h"
#include "game/assets.h"
#include "game/settings.h"
#include "game/menus/gameMenu.h"
#include "raymath.h"
#include "game/menus/uiUnits.h"
#include "rlgl.h"
#include "easing/easing.h"
#include "game/users/player.h"
#include "sol/sol.hpp"

Assets &gprAssets = Assets::getInstance();
Settings& gprSettings = Settings::getInstance();
AudioManager &gprAudioManager = AudioManager::getInstance();
PlayerManager &gprPlayerManager = PlayerManager::getInstance();
Menu& gprMenu = Menu::getInstance();
Units& gprU = Units::getInstance();

Color accentColor = {255,0,255,255};
float defaultHighwayLength = 11.5f;
Color OverdriveColor = {255,200,0,255};

#define LETTER_BOUNDRY_SIZE     0.25f
#define TEXT_MAX_LAYERS         32
#define LETTER_BOUNDRY_COLOR    VIOLET

bool SHOW_LETTER_BOUNDRY = false;
bool SHOW_TEXT_BOUNDRY = false;

// code from examples lol
static void DrawTextCodepoint3D(Font font, int codepoint, Vector3 position, float fontSize, bool backface, Color tint)
{
    // Character index position in sprite font
    // NOTE: In case a codepoint is not available in the font, index returned points to '?'
    int index = GetGlyphIndex(font, codepoint);
    float scale = fontSize/(float)font.baseSize;

    // Character destination rectangle on screen
    // NOTE: We consider charsPadding on drawing
    position.x += (float)(font.glyphs[index].offsetX - font.glyphPadding)/(float)font.baseSize*scale;
    position.z += (float)(font.glyphs[index].offsetY - font.glyphPadding)/(float)font.baseSize*scale;

    // Character source rectangle from font texture atlas
    // NOTE: We consider chars padding when drawing, it could be required for outline/glow shader effects
    Rectangle srcRec = { font.recs[index].x - (float)font.glyphPadding, font.recs[index].y - (float)font.glyphPadding,
                         font.recs[index].width + 2.0f*font.glyphPadding, font.recs[index].height + 2.0f*font.glyphPadding };

    float width = (float)(font.recs[index].width + 2.0f*font.glyphPadding)/(float)font.baseSize*scale;
    float height = (float)(font.recs[index].height + 2.0f*font.glyphPadding)/(float)font.baseSize*scale;

    if (font.texture.id > 0)
    {
        const float x = 0.0f;
        const float y = 0.0f;
        const float z = 0.0f;

        // normalized texture coordinates of the glyph inside the font texture (0.0f -> 1.0f)
        const float tx = srcRec.x/font.texture.width;
        const float ty = srcRec.y/font.texture.height;
        const float tw = (srcRec.x+srcRec.width)/font.texture.width;
        const float th = (srcRec.y+srcRec.height)/font.texture.height;

        if (SHOW_LETTER_BOUNDRY) {
	        DrawCubeWiresV(Vector3{ position.x + width/2, position.y, position.z + height/2}, Vector3{ width, LETTER_BOUNDRY_SIZE, height }, LETTER_BOUNDRY_COLOR);
        }

        rlCheckRenderBatchLimit(4 + 4*backface);
        rlSetTexture(font.texture.id);

        rlPushMatrix();
            rlTranslatef(position.x, position.y, position.z);

            rlBegin(RL_QUADS);
                rlColor4ub(tint.r, tint.g, tint.b, tint.a);

                // Front Face
                rlNormal3f(0.0f, 1.0f, 0.0f);                                   // Normal Pointing Up
                rlTexCoord2f(tx, ty); rlVertex3f(x,         y, z);              // Top Left Of The Texture and Quad
                rlTexCoord2f(tx, th); rlVertex3f(x,         y, z + height);     // Bottom Left Of The Texture and Quad
                rlTexCoord2f(tw, th); rlVertex3f(x + width, y, z + height);     // Bottom Right Of The Texture and Quad
                rlTexCoord2f(tw, ty); rlVertex3f(x + width, y, z);              // Top Right Of The Texture and Quad

                if (backface)
                {
                    // Back Face
                    rlNormal3f(0.0f, -1.0f, 0.0f);                              // Normal Pointing Down
                    rlTexCoord2f(tx, ty); rlVertex3f(x,         y, z);          // Top Right Of The Texture and Quad
                    rlTexCoord2f(tw, ty); rlVertex3f(x + width, y, z);          // Top Left Of The Texture and Quad
                    rlTexCoord2f(tw, th); rlVertex3f(x + width, y, z + height); // Bottom Left Of The Texture and Quad
                    rlTexCoord2f(tx, th); rlVertex3f(x,         y, z + height); // Bottom Right Of The Texture and Quad
                }
            rlEnd();
        rlPopMatrix();

        rlSetTexture(0);
    }
}

static void DrawText3D(Font font, const char *text, Vector3 position, float fontSize, float fontSpacing, float lineSpacing, bool backface, Color tint)
{
	int length = TextLength(text);          // Total length in bytes of the text, scanned by codepoints in loop

	float textOffsetY = 0.0f;               // Offset between lines (on line break '\n')
	float textOffsetX = 0.0f;               // Offset X to next character to draw

	float scale = fontSize/(float)font.baseSize;

	for (int i = 0; i < length;)
	{
		// Get next codepoint from byte string and glyph index in font
		int codepointByteCount = 0;
		int codepoint = GetCodepoint(&text[i], &codepointByteCount);
		int index = GetGlyphIndex(font, codepoint);

		// NOTE: Normally we exit the decoding sequence as soon as a bad byte is found (and return 0x3f)
		// but we need to draw all of the bad bytes using the '?' symbol moving one byte
		if (codepoint == 0x3f) codepointByteCount = 1;

		if (codepoint == '\n')
		{
			// NOTE: Fixed line spacing of 1.5 line-height
			// TODO: Support custom line spacing defined by user
			textOffsetY += scale + lineSpacing/(float)font.baseSize*scale;
			textOffsetX = 0.0f;
		}
		else
		{
			if ((codepoint != ' ') && (codepoint != '\t'))
			{
				DrawTextCodepoint3D(font, codepoint, Vector3{ position.x + textOffsetX, position.y, position.z + textOffsetY }, fontSize, backface, tint);
			}

			if (font.glyphs[index].advanceX == 0) textOffsetX += (float)(font.recs[index].width + fontSpacing)/(float)font.baseSize*scale;
			else textOffsetX += (float)(font.glyphs[index].advanceX + fontSpacing)/(float)font.baseSize*scale;
		}

		i += codepointByteCount;   // Move text bytes counter to next codepoint
	}
}

static Vector3 MeasureText3D(Font font, const char* text, float fontSize, float fontSpacing, float lineSpacing)
{
	int len = TextLength(text);
	int tempLen = 0;                // Used to count longer text line num chars
	int lenCounter = 0;

	float tempTextWidth = 0.0f;     // Used to count longer text line width

	float scale = fontSize/(float)font.baseSize;
	float textHeight = scale;
	float textWidth = 0.0f;

	int letter = 0;                 // Current character
	int index = 0;                  // Index position in sprite font

	for (int i = 0; i < len; i++)
	{
		lenCounter++;

		int next = 0;
		letter = GetCodepoint(&text[i], &next);
		index = GetGlyphIndex(font, letter);

		// NOTE: normally we exit the decoding sequence as soon as a bad byte is found (and return 0x3f)
		// but we need to draw all of the bad bytes using the '?' symbol so to not skip any we set next = 1
		if (letter == 0x3f) next = 1;
		i += next - 1;

		if (letter != '\n')
		{
			if (font.glyphs[index].advanceX != 0) textWidth += (font.glyphs[index].advanceX+fontSpacing)/(float)font.baseSize*scale;
			else textWidth += (font.recs[index].width + font.glyphs[index].offsetX)/(float)font.baseSize*scale;
		}
		else
		{
			if (tempTextWidth < textWidth) tempTextWidth = textWidth;
			lenCounter = 0;
			textWidth = 0.0f;
			textHeight += scale + lineSpacing/(float)font.baseSize*scale;
		}

		if (tempLen < lenCounter) tempLen = lenCounter;
	}

	if (tempTextWidth < textWidth) tempTextWidth = textWidth;

	Vector3 vec = { 0 };
	vec.x = tempTextWidth + (float)((tempLen - 1)*fontSpacing/(float)font.baseSize*scale); // Adds chars spacing to measure
	vec.y = 0.25f;
	vec.z = textHeight;

	return vec;
}

void gameplayRenderer::RenderNotes(Player* player, Chart& curChart, double time, RenderTexture2D &notes_tex, float length) {
	float diffDistance = player->Difficulty == 3 ? 2.0f : 1.5f;
	float lineDistance = player->Difficulty == 3 ? 1.5f : 1.0f;
	float DiffMultiplier = Remap(player->Difficulty, 0, 3, 1.0f, 1.75f);
	BeginTextureMode(notes_tex);
	ClearBackground({0,0,0,0});
	BeginMode3D(camera3pVector[cameraSel]);
	// glDisable(GL_CULL_FACE);
	for (int lane = 0; lane < (player->Difficulty == 3 ? 5 : 4); lane++) {
		for (int i = player->stats->curNoteIdx[lane]; i < curChart.notes_perlane[lane].size(); i++) {
			Color NoteColor;
			if (player->ClassicMode) {
				bool pd = (player->Instrument == 4);
				switch (lane) {
					case 0:
						if (pd) {
							NoteColor = ORANGE;
						} else {
							NoteColor = GREEN;
						}
					break;
					case 1:
						if (pd) {
							NoteColor = RED;
						} else {
							NoteColor = RED;
						}
					break;
					case 2:
						if (pd) {
							NoteColor = YELLOW;
						} else {
							NoteColor = YELLOW;
						}
					break;
					case 3:
						if (pd) {
							NoteColor = BLUE;
						} else {
							NoteColor = BLUE;
						}
					break;
					case 4:
						if (pd) {
							NoteColor = GREEN;
						} else {
							NoteColor = ORANGE;
						}
					break;
					default:
						NoteColor = accentColor;
					break;
				}
			}   else {
				NoteColor = gprMenu.hehe && player->Difficulty == 3 ? (lane == 0 || lane == 4 ? SKYBLUE : (lane == 1 || lane == 3 ? PINK : WHITE)) : accentColor;
			}

			// Color NoteColor = gprMenu.hehe && player->Difficulty == 3 ? (lane == 0 || lane == 4 ? SKYBLUE : (lane == 1 || lane == 3 ? PINK : WHITE)) : accentColor;
			Note & curNote = curChart.notes[curChart.notes_perlane[lane][i]];
			//if (curNote.hit) {
			//	player->stats->totalOffset += curNote.HitOffset;
			//}
			gprAssets.liftModel.materials[0].maps[MATERIAL_MAP_ALBEDO].color = NoteColor;

			gprAssets.noteTopModel.materials[0].maps[MATERIAL_MAP_ALBEDO].color = NoteColor;
			gprAssets.noteBottomModel.materials[0].maps[MATERIAL_MAP_ALBEDO].color = WHITE;
			if (!curChart.Solos.empty()) {
				if (curNote.time >= curChart.Solos[player->stats->curSolo].start &&
					curNote.time <= curChart.Solos[player->stats->curSolo].end) {
					if (curNote.hit) {
						if (curNote.hit && !curNote.countedForSolo) {
							curChart.Solos[player->stats->curSolo].notesHit++;
							curNote.countedForSolo = true;
						}
					}
					}
			}
			if (!curChart.odPhrases.empty()) {

				if (curNote.time >= curChart.odPhrases[player->stats->curODPhrase].start &&
					curNote.time < curChart.odPhrases[player->stats->curODPhrase].end &&
					!curChart.odPhrases[player->stats->curODPhrase].missed) {
					if (curNote.hit) {
						if (curNote.hit && !curNote.countedForODPhrase) {
							curChart.odPhrases[player->stats->curODPhrase].notesHit++;
							curNote.countedForODPhrase = true;
						}
					}
					curNote.renderAsOD = true;

					}
				if (curChart.odPhrases[player->stats->curODPhrase].missed) {
					curNote.renderAsOD = false;
				}
				if (curChart.odPhrases[player->stats->curODPhrase].notesHit ==
					curChart.odPhrases[player->stats->curODPhrase].noteCount &&
					!curChart.odPhrases[player->stats->curODPhrase].added && player->stats->overdriveFill < 1.0f) {
					player->stats->overdriveFill += 0.25f;
					if (player->stats->overdriveFill > 1.0f) player->stats->overdriveFill = 1.0f;
					if (player->stats->Overdrive) {
						player->stats->overdriveActiveFill = player->stats->overdriveFill;
						player->stats->overdriveActiveTime = time;
					}
					curChart.odPhrases[player->stats->curODPhrase].added = true;
					}
			}
			if (!curNote.hit && !curNote.accounted && curNote.time + goodBackend < time && !songEnded) {
				curNote.miss = true;
				player->stats->MissNote();
				if (!curChart.odPhrases.empty() && !curChart.odPhrases[player->stats->curODPhrase].missed &&
					curNote.time >= curChart.odPhrases[player->stats->curODPhrase].start &&
					curNote.time < curChart.odPhrases[player->stats->curODPhrase].end) {
					curChart.odPhrases[player->stats->curODPhrase].missed = true;
					};
				player->stats->Combo = 0;
				curNote.accounted = true;
			} else if (player->Bot) {
				if (!curNote.hit && !curNote.accounted && curNote.time < time &&
					player->stats->curNoteInt < curChart.notes.size() && !songEnded) {
					curNote.hit = true;
					player->stats->HitNote(false);
					if (gprPlayerManager.BandStats.Multiplayer) {
						gprPlayerManager.BandStats.AddNotePoint(curNote.perfect, player->stats->noODmultiplier());
					}
					if (curNote.len > 0) curNote.held = true;
					curNote.accounted = true;
					// player->stats->Notes += 1;
					// player->stats->Combo++;
					curNote.accounted = true;
					curNote.hitTime = time;
					}
			}

			double relTime = ((curNote.time - time)) *
							 (player->NoteSpeed * DiffMultiplier) * (11.5f / length);
			double relEnd = (((curNote.time + curNote.len) - time)) *
							(player->NoteSpeed * DiffMultiplier) * (11.5f / length);
			float notePosX = diffDistance - (1.0f *
											 (float) (player->LeftyFlip ? (player->Difficulty == 3 ? 4 : 3) -
																			   curNote.lane
																			 : curNote.lane));
			if (relTime > 1.5) {
				break;
			}
			if (relEnd > 1.5) relEnd = 1.5;
			if (curNote.lift && !curNote.hit && !curNote.miss) {
				// lifts						//  distance between notes
				//									(furthest left - lane distance)
				if (curNote.renderAsOD)                    //  1.6f	0.8
					DrawModel(gprAssets.liftModelOD, Vector3{notePosX, 0, smasherPos +
																		  (length *
																		   (float) relTime)}, 1.1f, WHITE);
				// energy phrase
				else
					DrawModel(gprAssets.liftModel, Vector3{notePosX, 0, smasherPos +
																		(length * (float) relTime)},
							  1.1f, WHITE);
				// regular
			} else {
				// sustains
				if ((curNote.len) > 0) {
					if (curNote.hit && curNote.held) {
						if (curNote.heldTime <
							(curNote.len * player->NoteSpeed)) {
							curNote.heldTime = 0.0 - relTime;
							//if (!bot) {
							//player.sustainScoreBuffer[curNote.lane] =
							//		(float) (curNote.heldTime / curNote.len) * (12 * curNote.beatsLen) *
							//		player.multiplier(player->Instrument);
							//}
							if (relTime < 0.0) relTime = 0.0;
							}
						if (relEnd <= 0.0) {
							if (relTime < 0.0) relTime = relEnd;
							//if (!bot) {
							//player.score += player.sustainScoreBuffer[curNote.lane];
							//player.sustainScoreBuffer[curNote.lane] = 0;
							//}
							curNote.held = false;
						}
					} else if (curNote.hit && !curNote.held) {
						relTime = relTime + curNote.heldTime;
					}

					/*Color SustainColor = Color{ 69,69,69,255 };
					if (curNote.held) {
						if (od) {
							Color SustainColor = Color{ 217, 183, 82 ,255 };
						}
						Color SustainColor = Color{ 172,82,217,255 };
					}*/
					float sustainLen =
							(length * (float) relEnd) - (length * (float) relTime);
					Matrix sustainMatrix = MatrixMultiply(MatrixScale(1, 1, sustainLen),
														  MatrixTranslate(notePosX, 0.01f,
																		  smasherPos +
																		  (length *
																		   (float) relTime) +
																		  (sustainLen / 2.0f)));
					BeginBlendMode(BLEND_ALPHA);
					gprAssets.sustainMat.maps[MATERIAL_MAP_DIFFUSE].color = ColorTint(NoteColor, { 180,180,180,255 });
					gprAssets.sustainMatHeld.maps[MATERIAL_MAP_DIFFUSE].color = ColorBrightness(NoteColor, 0.5f);


					if (curNote.held && !curNote.renderAsOD) {
						DrawMesh(sustainPlane, gprAssets.sustainMatHeld, sustainMatrix);
						DrawCube(Vector3{notePosX, 0.1, smasherPos}, 0.4f, 0.2f, 0.4f,
								 accentColor);
						//DrawCylinderEx(Vector3{ notePosX, 0.05f, smasherPos + (highwayLength * (float)relTime) }, Vector3{ notePosX,0.05f, smasherPos + (highwayLength * (float)relEnd) }, 0.1f, 0.1f, 15, accentColor);
					}
					if (curNote.renderAsOD && curNote.held) {
						DrawMesh(sustainPlane, gprAssets.sustainMatHeldOD, sustainMatrix);
						DrawCube(Vector3{notePosX, 0.1, smasherPos}, 0.4f, 0.2f, 0.4f, WHITE);
						//DrawCylinderEx(Vector3{ notePosX, 0.05f, smasherPos + (highwayLength * (float)relTime) }, Vector3{ notePosX,0.05f, smasherPos + (highwayLength * (float)relEnd) }, 0.1f, 0.1f, 15, Color{ 255, 255, 255 ,255 });
					}
					if (!curNote.held && curNote.hit || curNote.miss) {

						DrawMesh(sustainPlane, gprAssets.sustainMatMiss, sustainMatrix);
						//DrawCylinderEx(Vector3{ notePosX, 0.05f, smasherPos + (highwayLength * (float)relTime) }, Vector3{ notePosX,0.05f, smasherPos + (highwayLength * (float)relEnd) }, 0.1f, 0.1f, 15, Color{ 69,69,69,255 });
					}
					if (!curNote.hit && !curNote.accounted && !curNote.miss) {
						if (curNote.renderAsOD) {
							DrawMesh(sustainPlane, gprAssets.sustainMatOD, sustainMatrix);
							//DrawCylinderEx(Vector3{ notePosX, 0.05f, smasherPos + (highwayLength * (float)relTime) }, Vector3{ notePosX,0.05f, smasherPos + (highwayLength * (float)relEnd) }, 0.1f, 0.1f, 15, Color{ 200, 200, 200 ,255 });
						} else {
							DrawMesh(sustainPlane, gprAssets.sustainMat, sustainMatrix);
							/*DrawCylinderEx(Vector3{notePosX, 0.05f,
													smasherPos + (highwayLength * (float)relTime) },
										   Vector3{ notePosX, 0.05f,
													smasherPos + (highwayLength * (float)relEnd) }, 0.1f, 0.1f, 15,
										   accentColor);*/
						}
					}
					EndBlendMode();

					// DrawLine3D(Vector3{ diffDistance - (1.0f * curNote.lane),0.05f,smasherPos + (12.5f * (float)relTime) }, Vector3{ diffDistance - (1.0f * curNote.lane),0.05f,smasherPos + (12.5f * (float)relEnd) }, Color{ 172,82,217,255 });
				}
				// regular notes
				if (((curNote.len) > 0 && (curNote.held || !curNote.hit)) ||
					((curNote.len) == 0 && !curNote.hit)) {
					if (curNote.renderAsOD) {
						if ((!curNote.held && !curNote.miss) && !curNote.hit) {
							DrawModel(gprAssets.noteTopModelOD, Vector3{notePosX, 0, smasherPos +
																					 (length *
																					  (float) relTime)}, 1.1f,
									  WHITE);
							DrawModel(gprAssets.noteBottomModelOD, Vector3{notePosX, 0, smasherPos +
																						(length *
																						 (float) relTime)}, 1.1f,
									  WHITE);
						}

					} else {
						if ((!curNote.held && !curNote.miss) && !curNote.hit) {
							DrawModel(gprAssets.noteTopModel, Vector3{notePosX, 0, smasherPos +
																				   (length *
																					(float) relTime)}, 1.1f,
									  WHITE);
							DrawModel(gprAssets.noteBottomModel, Vector3{notePosX, 0, smasherPos +
																					  (length *
																					   (float) relTime)}, 1.1f,
									  WHITE);
						}

					}

					}
				gprAssets.expertHighway.materials[0].maps[MATERIAL_MAP_ALBEDO].color = accentColor;
			}
			if (curNote.miss) {


				if (curNote.lift) {
					DrawModel(gprAssets.liftModel,
							  Vector3{notePosX, 0, smasherPos + (length * (float) relTime)},
							  1.0f, RED);
				} else {
					DrawModel(gprAssets.noteBottomModel,
							  Vector3{notePosX, 0, smasherPos + (length * (float) relTime)},
							  1.0f, RED);
					DrawModel(gprAssets.noteTopModel,
							  Vector3{notePosX, 0, smasherPos + (length * (float) relTime)},
							  1.0f, RED);
				}


				if (gprAudioManager.GetMusicTimePlayed(gprAudioManager.loadedStreams[0].handle) <
					curNote.time + 0.4 && gprSettings.missHighwayColor) {
					gprAssets.expertHighway.materials[0].maps[MATERIAL_MAP_ALBEDO].color = RED;
					} else {
						gprAssets.expertHighway.materials[0].maps[MATERIAL_MAP_ALBEDO].color = accentColor;
					}
			}
			BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
			double PerfectHitAnimDuration = 1.0f;
			if (curNote.hit && gprAudioManager.GetMusicTimePlayed(gprAudioManager.loadedStreams[0].handle) < (curNote.hitTime) + HitAnimDuration) {
				double TimeSinceHit = gprAudioManager.GetMusicTimePlayed(gprAudioManager.loadedStreams[0].handle) - curNote.hitTime;
				unsigned char HitAlpha = Remap(getEasingFunction(EaseOutExpo)(TimeSinceHit/HitAnimDuration), 0, 1.0, 196, 0);
				DrawCube(Vector3{notePosX, 0.125, smasherPos}, 1.0f, 0.25f, 0.5f,
						 curNote.perfect ? Color{255, 215, 0, HitAlpha} : Color{255, 255, 255, HitAlpha});
			}
			EndBlendMode();
			if (curNote.hit && gprAudioManager.GetMusicTimePlayed(gprAudioManager.loadedStreams[0].handle) <
							(curNote.hitTime) + PerfectHitAnimDuration && curNote.perfect) {

				double TimeSinceHit = gprAudioManager.GetMusicTimePlayed(gprAudioManager.loadedStreams[0].handle) - curNote.hitTime;
				unsigned char HitAlpha = Remap(getEasingFunction(EaseOutQuad)(TimeSinceHit/PerfectHitAnimDuration), 0, 1.0, 255, 0);
				float HitPosLeft = Remap(getEasingFunction(EaseInOutBack)(TimeSinceHit/PerfectHitAnimDuration), 0, 1.0, 3.4, 3.0);

				DrawCube(Vector3{HitPosLeft, -0.1f, smasherPos}, 1.0f, 0.01f,
						 0.5f, Color{255,161,0,HitAlpha});
				DrawCube(Vector3{HitPosLeft, -0.11f, smasherPos}, 1.0f, 0.01f,
						 1.0f, Color{255,161,0,(unsigned char)(HitAlpha/2)});
							}
			// DrawText3D(gprAssets.rubik, TextFormat("%01i", combo), Vector3{2.8f, 0, smasherPos}, 32, 0.5,0,false,FC ? GOLD : (combo <= 3) ? RED : WHITE);


			if (relEnd < -1 && player->stats->curNoteIdx[lane] < curChart.notes_perlane[lane].size() - 1)
				player->stats->curNoteIdx[lane] = i + 1;

		}

	}


	EndMode3D();
	EndTextureMode();

	SetTextureWrap(notes_tex.texture,TEXTURE_WRAP_CLAMP);
	notes_tex.texture.width = (float)GetScreenWidth();
	notes_tex.texture.height = (float)GetScreenHeight();
	DrawTexturePro(notes_tex.texture, {0,0,(float)GetScreenWidth(), (float)-GetScreenHeight() },{ 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight() }, {renderPos,highwayLevel}, 0, WHITE );
}

void gameplayRenderer::RenderClassicNotes(Player* player, Chart& curChart, double time, RenderTexture2D &notes_tex, float length) {
	float diffDistance = 2.0f;
	float lineDistance = 1.5f;
	float DiffMultiplier = Remap(player->Difficulty, 0, 3, 1.0f, 1.75f);
	BeginTextureMode(notes_tex);
	ClearBackground({0,0,0,0});
	BeginMode3D(camera3pVector[cameraSel]);
	// glDisable(GL_CULL_FACE);


	for (auto & curNote : curChart.notes) {

		if (!curChart.Solos.empty()) {
			if (curNote.time >= curChart.Solos[player->stats->curSolo].start &&
				curNote.time < curChart.Solos[player->stats->curSolo].end) {
				if (curNote.hit) {
					if (curNote.hit && !curNote.countedForSolo) {
						curChart.Solos[player->stats->curSolo].notesHit++;
						curNote.countedForSolo = true;
					}
				}
			}
		}
		if (!curChart.odPhrases.empty()) {

			if (curNote.time >= curChart.odPhrases[player->stats->curODPhrase].start &&
				curNote.time < curChart.odPhrases[player->stats->curODPhrase].end &&
				!curChart.odPhrases[player->stats->curODPhrase].missed) {
				if (curNote.hit) {
					if (curNote.hit && !curNote.countedForODPhrase) {
						curChart.odPhrases[player->stats->curODPhrase].notesHit++;
						curNote.countedForODPhrase = true;
					}
				}
				curNote.renderAsOD = true;

			}
			if (curChart.odPhrases[player->stats->curODPhrase].missed) {
				curNote.renderAsOD = false;
			}
			if (curChart.odPhrases[player->stats->curODPhrase].notesHit ==
				curChart.odPhrases[player->stats->curODPhrase].noteCount &&
				!curChart.odPhrases[player->stats->curODPhrase].added && player->stats->overdriveFill < 1.0f) {
				player->stats->overdriveFill += 0.25f;
				if (player->stats->overdriveFill > 1.0f) player->stats->overdriveFill = 1.0f;
				if (player->stats->Overdrive) {
					player->stats->overdriveActiveFill = player->stats->overdriveFill;
					player->stats->overdriveActiveTime = time;
				}
				curChart.odPhrases[player->stats->curODPhrase].added = true;
			}
		}
		if (!curNote.hit && !curNote.accounted && curNote.time + goodBackend < time &&
			!songEnded && player->stats->curNoteInt < curChart.notes.size() && !songEnded && !player->Bot) {
			TraceLog(LOG_INFO, TextFormat("Missed note at %f, note %01i", time, player->stats->curNoteInt));
			curNote.miss = true;
			FAS = false;
			player->stats->MissNote();
			if (!curChart.odPhrases.empty() && !curChart.odPhrases[player->stats->curODPhrase].missed &&
				curNote.time >= curChart.odPhrases[player->stats->curODPhrase].start &&
				curNote.time < curChart.odPhrases[player->stats->curODPhrase].end)
				curChart.odPhrases[player->stats->curODPhrase].missed = true;
			player->stats->Combo = 0;
			curNote.accounted = true;
			player->stats->curNoteInt++;
		} else if (player->Bot) {
			if (!curNote.hit && !curNote.accounted && curNote.time < time && player->stats->curNoteInt < curChart.notes.size() && !songEnded) {
				curNote.hit = true;
				player->stats->HitNote(false);
				if (gprPlayerManager.BandStats.Multiplayer) {
					gprPlayerManager.BandStats.AddNotePoint(curNote.perfect, player->stats->noODmultiplier());
				}
				// player->stats->Notes++;
				if (curNote.len > 0) curNote.held = true;
				curNote.accounted = true;
				// player->stats->Combo++;
				curNote.accounted = true;
				curNote.hitTime = time;
				player->stats->curNoteInt++;
			}
		}

		double relTime = ((curNote.time - time)) *
						 (player->NoteSpeed * DiffMultiplier) * (11.5f / length);
		double relEnd = (((curNote.time + curNote.len) - time)) *
						(player->NoteSpeed * DiffMultiplier) * (11.5f / length);

		float hopoScale = curNote.phopo ? 0.75f : 1.1f;

		for (int lane: curNote.pLanes) {

			int noteLane = gprSettings.mirrorMode ? 4 - lane : lane;

			Color NoteColor;
			switch (lane) {
				case 0:
					NoteColor = gprMenu.hehe ? SKYBLUE : GREEN;
					break;
				case 1:
					NoteColor = gprMenu.hehe ? PINK : RED;
					break;
				case 2:
					NoteColor = gprMenu.hehe ? RAYWHITE : YELLOW;
					break;
				case 3:
					NoteColor = gprMenu.hehe ? PINK : BLUE;
					break;
				case 4:
					NoteColor = gprMenu.hehe ? SKYBLUE : ORANGE;
					break;
				default:
					NoteColor = accentColor;
					break;
			}

			gprAssets.noteTopModel.materials[0].maps[MATERIAL_MAP_ALBEDO].color = NoteColor;
			gprAssets.noteTopModelHP.materials[0].maps[MATERIAL_MAP_ALBEDO].color = NoteColor;

			float notePosX = diffDistance - (1.0f * noteLane);
			if (relTime > 1.5) {
				break;
			}
			if (relEnd > 1.5) relEnd = 1.5;
			if ((curNote.phopo || curNote.pTap) && !curNote.hit && !curNote.miss) {
				if (curNote.renderAsOD) {
					gprAssets.noteTopModelHP.materials[0].maps[MATERIAL_MAP_ALBEDO].color = GOLD;
					gprAssets.noteBottomModelHP.materials[0].maps[MATERIAL_MAP_ALBEDO].color = WHITE;
					DrawModel(gprAssets.noteTopModelHP, Vector3{notePosX, 0, smasherPos +
																			 (length *
																			  (float) relTime)}, 1.1f,
							  WHITE);
					DrawModel(gprAssets.noteBottomModelHP, Vector3{notePosX, 0, smasherPos +
																				(length *
																				 (float) relTime)}, 1.1f,
							  WHITE);
				} else {
					gprAssets.noteTopModelHP.materials[0].maps[MATERIAL_MAP_ALBEDO].color = curNote.pTap ? BLACK : NoteColor;
					gprAssets.noteBottomModelHP.materials[0].maps[MATERIAL_MAP_ALBEDO].color = curNote.pTap ? NoteColor : WHITE;
					DrawModel(gprAssets.noteTopModelHP, Vector3{notePosX, 0, smasherPos +
																			 (length *
																			  (float) relTime)}, 1.1f,
							  WHITE);
					DrawModel(gprAssets.noteBottomModelHP, Vector3{notePosX, 0, smasherPos +
																				(length *
																				 (float) relTime)}, 1.1f,
							  WHITE);
				}
			} else if (curNote.miss && (curNote.phopo || curNote.pTap)) {
				gprAssets.noteTopModelHP.materials[0].maps[MATERIAL_MAP_ALBEDO].color = RED;
				gprAssets.noteBottomModelHP.materials[0].maps[MATERIAL_MAP_ALBEDO].color = RED;
				DrawModel(gprAssets.noteTopModelHP, Vector3{notePosX, 0, smasherPos +
																		 (length *
																		  (float) relTime)}, 1.1f,
						  WHITE);
				DrawModel(gprAssets.noteBottomModelHP, Vector3{notePosX, 0, smasherPos +
																			(length *
																			 (float) relTime)}, 1.1f,
						  WHITE);
			}
			if ((curNote.len) > 0) {
				int pressedMask = 0b000000;

				for (int pressedButtons = 0; pressedButtons < player->stats->HeldFrets.size(); pressedButtons++) {
					if (player->stats->HeldFrets[pressedButtons] || player->stats->HeldFretsAlt[pressedButtons])
						pressedMask += curChart.PlasticFrets[pressedButtons];
				}

				Note &lastNote = curChart.notes[player->stats->curNoteInt == 0 ? 0 : player->stats->curNoteInt - 1];
				bool chordMatch = (extendedSustainActive ? pressedMask >= curNote.mask : pressedMask == curNote.mask);
				bool singleMatch = (extendedSustainActive ? pressedMask >= curNote.mask : pressedMask >= curNote.mask && pressedMask < (curNote.mask * 2));
				bool noteMatch = (curNote.chord ? chordMatch : singleMatch);

				if (curNote.extendedSustain)
					TraceLog(LOG_INFO, "extended sustain lol");

				if ((!noteMatch && curNote.held) && curNote.time + curNote.len + 0.1 > time && !player->Bot) {
					curNote.held = false;
					if (curNote.extendedSustain == true)
						extendedSustainActive = false;
				}

				if ((curNote.hit && curNote.held) && curNote.time + curNote.len + 0.1 > time) {
					if (curNote.heldTime < (curNote.len * (player->NoteSpeed * DiffMultiplier))) {
						curNote.heldTime = 0.0 - relTime;
						player->stats->Score +=
						        (float) (curNote.heldTime / curNote.len) * (12 * curNote.beatsLen) *
						        player->stats->multiplier();
						if (relTime < 0.0) relTime = 0.0;
					}
					if (relEnd <= 0.0) {
						if (relTime < 0.0) relTime = relEnd;
						curNote.held = false;
						if (curNote.extendedSustain == true)
							extendedSustainActive = false;
					}
				} else if (curNote.hit && !curNote.held && !player->Bot) {
					relTime = relTime + curNote.heldTime;
				}
				float sustainLen =
						(length * (float) relEnd) - (length * (float) relTime);
				Matrix sustainMatrix = MatrixMultiply(MatrixScale(1, 1, sustainLen),
													  MatrixTranslate(notePosX, 0.01f,
																	  smasherPos +
																	  (length *
																	   (float) relTime) +
																	  (sustainLen / 2.0f)));
				BeginBlendMode(BLEND_ALPHA);
				gprAssets.sustainMat.maps[MATERIAL_MAP_DIFFUSE].color = ColorTint(NoteColor, {180, 180, 180, 255});
				gprAssets.sustainMatHeld.maps[MATERIAL_MAP_DIFFUSE].color = ColorBrightness(NoteColor, 0.5f);


				if (curNote.held && !curNote.renderAsOD) {
					DrawMesh(sustainPlane, gprAssets.sustainMatHeld, sustainMatrix);
					DrawCube(Vector3{notePosX, 0.1, smasherPos}, 0.4f, 0.2f, 0.4f,
							 accentColor);
					//DrawCylinderEx(Vector3{ notePosX, 0.05f, smasherPos + (highwayLength * (float)relTime) }, Vector3{ notePosX,0.05f, smasherPos + (highwayLength * (float)relEnd) }, 0.1f, 0.1f, 15, accentColor);
				}
				if (curNote.renderAsOD && curNote.held) {
					DrawMesh(sustainPlane, gprAssets.sustainMatHeldOD, sustainMatrix);
					DrawCube(Vector3{notePosX, 0.1, smasherPos}, 0.4f, 0.2f, 0.4f, WHITE);
					//DrawCylinderEx(Vector3{ notePosX, 0.05f, smasherPos + (highwayLength * (float)relTime) }, Vector3{ notePosX,0.05f, smasherPos + (highwayLength * (float)relEnd) }, 0.1f, 0.1f, 15, Color{ 255, 255, 255 ,255 });
				}
				if (!curNote.held && curNote.hit || curNote.miss) {

					DrawMesh(sustainPlane, gprAssets.sustainMatMiss, sustainMatrix);
					//DrawCylinderEx(Vector3{ notePosX, 0.05f, smasherPos + (highwayLength * (float)relTime) }, Vector3{ notePosX,0.05f, smasherPos + (highwayLength * (float)relEnd) }, 0.1f, 0.1f, 15, Color{ 69,69,69,255 });
				}
				if (!curNote.hit && !curNote.accounted && !curNote.miss) {
					if (curNote.renderAsOD) {
						DrawMesh(sustainPlane, gprAssets.sustainMatOD, sustainMatrix);
						//DrawCylinderEx(Vector3{ notePosX, 0.05f, smasherPos + (highwayLength * (float)relTime) }, Vector3{ notePosX,0.05f, smasherPos + (highwayLength * (float)relEnd) }, 0.1f, 0.1f, 15, Color{ 200, 200, 200 ,255 });
					} else {
						DrawMesh(sustainPlane, gprAssets.sustainMat, sustainMatrix);
						/*DrawCylinderEx(Vector3{notePosX, 0.05f,
												smasherPos + (highwayLength * (float)relTime) },
									   Vector3{ notePosX, 0.05f,
												smasherPos + (highwayLength * (float)relEnd) }, 0.1f, 0.1f, 15,
									   accentColor);*/
					}
				}
				EndBlendMode();
			}
			if ((!curNote.phopo  && ((curNote.len) > 0 && (curNote.held || !curNote.hit)) ||
				((curNote.len) == 0 && !curNote.hit) && !curNote.phopo && !curNote.pTap) && !curNote.miss) {
				if (curNote.renderAsOD) {
					if ((!curNote.held && !curNote.miss && !curNote.phopo) && !curNote.hit) {
						DrawModel(gprAssets.noteTopModelOD, Vector3{notePosX, 0, smasherPos +
																				 (length *
																				  (float) relTime)}, 1.1f,
								  WHITE);
						DrawModel(gprAssets.noteBottomModelOD, Vector3{notePosX, 0, smasherPos +
																					(length *
																					 (float) relTime)}, 1.1f,
								  WHITE);
					}

				} else {
					if ((!curNote.held && !curNote.miss && !curNote.pTap && !curNote.phopo) && !curNote.hit) {
						DrawModel(gprAssets.noteTopModel, Vector3{notePosX, 0, smasherPos +
																			   (length *
																				(float) relTime)}, 1.1f,
								  WHITE);
						DrawModel(gprAssets.noteBottomModel, Vector3{notePosX, 0, smasherPos +
																				  (length *
																				   (float) relTime)}, 1.1f,
								  WHITE);
					}
				}

			} else if ((!curNote.phopo && !curNote.pTap) && curNote.miss) {
				gprAssets.noteTopModel.materials[0].maps[MATERIAL_MAP_ALBEDO].color = RED;
				DrawModel(gprAssets.noteTopModel, Vector3{notePosX, 0, smasherPos +
																			   (length *
																				(float) relTime)}, 1.1f,
								  RED);
				DrawModel(gprAssets.noteBottomModel, Vector3{notePosX, 0, smasherPos +
																		  (length *
																		   (float) relTime)}, 1.1f,
						  RED);
			}

			if (curNote.miss && curNote.time + 0.5 < time) {
				if (curNote.phopo) {
					DrawModel(gprAssets.noteBottomModelHP,
							  Vector3{notePosX, 0, smasherPos + (length * (float) relTime)},
							  1.0f, RED);
					DrawModel(gprAssets.noteTopModelHP,
							  Vector3{notePosX, 0, smasherPos + (length * (float) relTime)},
							  1.0f, RED);
				} else {
					DrawModel(gprAssets.noteBottomModel,
							  Vector3{notePosX, 0, smasherPos + (length * (float) relTime)},
							  1.0f, RED);
					DrawModel(gprAssets.noteTopModel,
							  Vector3{notePosX, 0, smasherPos + (length * (float) relTime)},
							  1.0f, RED);
				}


				if (gprAudioManager.GetMusicTimePlayed(gprAudioManager.loadedStreams[0].handle) <
					curNote.time + 0.4 && gprSettings.missHighwayColor) {
					gprAssets.expertHighway.materials[0].maps[MATERIAL_MAP_ALBEDO].color = RED;
				} else {
					gprAssets.expertHighway.materials[0].maps[MATERIAL_MAP_ALBEDO].color = accentColor;
				}
			}

			double PerfectHitAnimDuration = 1.0f;
			if (curNote.hit && gprAudioManager.GetMusicTimePlayed(gprAudioManager.loadedStreams[0].handle) <
							   curNote.hitTime + HitAnimDuration) {

				double TimeSinceHit = gprAudioManager.GetMusicTimePlayed(gprAudioManager.loadedStreams[0].handle) - curNote.hitTime;
				unsigned char HitAlpha = Remap(getEasingFunction(EaseInBack)(TimeSinceHit/HitAnimDuration), 0, 1.0, 196, 0);

				DrawCube(Vector3{notePosX, 0.125, smasherPos}, 1.0f, 0.25f, 0.5f,
						 curNote.perfect ? Color{255, 215, 0, HitAlpha} : Color{255, 255, 255, HitAlpha});
				// if (curNote.perfect) {
				// 	DrawCube(Vector3{3.3f, 0, smasherPos}, 1.0f, 0.01f,
				// 			 0.5f, Color{255,161,0,HitAlpha});
				// }


			}

			if (curNote.hit && gprAudioManager.GetMusicTimePlayed(gprAudioManager.loadedStreams[0].handle) <
							   curNote.hitTime + PerfectHitAnimDuration && curNote.perfect) {

				double TimeSinceHit = gprAudioManager.GetMusicTimePlayed(gprAudioManager.loadedStreams[0].handle) - curNote.hitTime;
				unsigned char HitAlpha = Remap(getEasingFunction(EaseOutQuad)(TimeSinceHit/PerfectHitAnimDuration), 0, 1.0, 255, 0);
				float HitPosLeft = Remap(getEasingFunction(EaseInOutBack)(TimeSinceHit/PerfectHitAnimDuration), 0, 1.0, 3.4, 3.0);

				DrawCube(Vector3{HitPosLeft, -0.1f, smasherPos}, 1.0f, 0.01f,
						 0.5f, Color{255,161,0,HitAlpha});
				DrawCube(Vector3{HitPosLeft, -0.11f, smasherPos}, 1.0f, 0.01f,
						 1.0f, Color{255,161,0,(unsigned char)(HitAlpha/2)});
			}
		}
	}
	EndMode3D();
	EndTextureMode();

	SetTextureWrap(notes_tex.texture,TEXTURE_WRAP_CLAMP);
	notes_tex.texture.width = (float)GetScreenWidth();
	notes_tex.texture.height = (float)GetScreenHeight();
	DrawTexturePro(notes_tex.texture, {0,0,(float)GetScreenWidth(), (float)-GetScreenHeight() },{ 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight() }, {renderPos,highwayLevel}, 0, WHITE );
}

void gameplayRenderer::RenderHud(Player* player, RenderTexture2D& hud_tex, float length) {
	BeginTextureMode(hud_tex);
	float DiffMultiplier = Remap(player->Difficulty, 0, 3, 1.0f, 1.75f);
	ClearBackground({0,0,0,0});
	BeginMode3D(camera3pVector[cameraSel]);
	if (showHitwindow) {
		BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
		float lineDistance = player->Difficulty == 3 ? 1.5f : 1.0f;
		float highwayLength = defaultHighwayLength * gprSettings.highwayLengthMult;
		float highwayPosShit = ((20) * (1 - gprSettings.highwayLengthMult));
		double hitwindowFront = ((0.1)) * (player->NoteSpeed * DiffMultiplier) * (11.5f / length);
		double hitwindowBack = ((-0.1)) * (player->NoteSpeed * DiffMultiplier) * (11.5f / length);
		bool Beginning = (float) (smasherPos + (length * hitwindowFront)) >= (length * 1.5f) + smasherPos;
		bool Ending = (float) (smasherPos + (length * hitwindowBack)) >= (length * 1.5f) + smasherPos;

		float Front = (float) (smasherPos + (length * hitwindowFront));
		float Back = (float) (smasherPos + (length * hitwindowBack));

		DrawTriangle3D({0 - lineDistance - 1.0f, 0.003, Back},
					   {0 - lineDistance - 1.0f, 0.003, Front},
					   {lineDistance + 1.0f, 0.003, Back},
					   Color{96, 96, 96, 64});

		DrawTriangle3D({lineDistance + 1.0f, 0.003, Front},
					   {lineDistance + 1.0f, 0.003, Back},
					   {0 - lineDistance - 1.0f, 0.003, Front},
					   Color{96, 96, 96, 64});

		double perfectFront = ((0.025f)) * (player->NoteSpeed * DiffMultiplier) * (11.5f / length);
		double perfectBack = ((-0.025f)) * (player->NoteSpeed * DiffMultiplier) * (11.5f / length);
		bool pBeginning = (float) (smasherPos + (length * perfectFront)) >= (length * 1.5f) + smasherPos;
		bool pEnding = (float) (smasherPos + (length * perfectBack)) >= (length * 1.5f) + smasherPos;

		float pFront = (float) (smasherPos + (length * perfectFront));
		float pBack = (float) (smasherPos + (length * perfectBack));

		DrawTriangle3D({0 - lineDistance - 1.0f, 0.006, pBack},
					   {0 - lineDistance - 1.0f, 0.006, pFront},
					   {lineDistance + 1.0f, 0.006, pBack},
					   Color{32, 32, 32, 8});

		DrawTriangle3D({lineDistance + 1.0f, 0.006, pFront},
					   {lineDistance + 1.0f, 0.006, pBack},
					   {0 - lineDistance - 1.0f, 0.006, pFront},
					   Color{32, 32, 32, 8});
		BeginBlendMode(BLEND_ALPHA);
	}
	DrawModel(gprAssets.odFrame, Vector3{ 0,1.0f,-0.3f }, 0.8f, WHITE);
	DrawModel(gprAssets.odBar, Vector3{ 0,1.0f,-0.3f }, 0.8f, WHITE);

	// DrawModel(gprAssets.MultInnerDot, Vector3{0,1.0f,-0.3}, 1, WHITE);
	// DrawModel(gprAssets.MultFill, Vector3{0,1.0f,-0.3}, 1, WHITE);
	// DrawModel(gprAssets.MultOuterFrame, Vector3{0,1.0f,-0.3}, 1, WHITE);
	// DrawModel(gprAssets.MultInnerFrame, Vector3{0,1.0f,-0.3}, 1, WHITE);

	DrawModel(gprAssets.multFrame, Vector3{ 0,1.0f,-0.3f }, 0.8f, WHITE);
	DrawModel(gprAssets.multBar, Vector3{ 0,1.0f,-0.3f }, 0.8f, WHITE);
	if (player->Instrument == 1 || player->Instrument == 3 || player->Instrument == 5) {

		DrawModel(gprAssets.multCtr5, Vector3{ 0,1.0f,-0.3f }, 0.8f, WHITE);
	}
	else {

		DrawModel(gprAssets.multCtr3, Vector3{ 0,1.0f,-0.3f }, 0.8f, WHITE);
	}
	DrawModel(gprAssets.multNumber, Vector3{ 0,1.0f,-0.3f }, 0.8f, WHITE);

	float angle = 90;
	float rotateX = 0;
	float rotateY = 1;
	float rotateZ = 0;


	float posX = -7.5;
	float posY = 0;
	float posZ = -3.3;

	float scaleX = 2;
	float scaleY = 1;
	float scaleZ = 1;

	float fontSize = 80;

	rlPushMatrix();
		rlRotatef(angle, rotateX, rotateY, rotateZ);
		rlScalef(scaleX, scaleY, scaleZ);
		//rlRotatef(90.0f, 0.0f, 0.0f, 1.0f);								//0, 1, 2.4
		DrawText3D(gprAssets.rubikBold, player->Name.c_str(), Vector3{ posX,posY,posZ }, fontSize, 0, 0, 1, WHITE);
	rlPopMatrix();


	EndMode3D();
	EndTextureMode();

	SetTextureWrap(hud_tex.texture,TEXTURE_WRAP_CLAMP);
	hud_tex.texture.width = (float)GetScreenWidth();
	hud_tex.texture.height = (float)GetScreenHeight();
	DrawTexturePro(hud_tex.texture, {0,0,(float)GetScreenWidth(), (float)-GetScreenHeight() },{ 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight() }, {renderPos,highwayLevel}, 0, WHITE );
}

void gameplayRenderer::RaiseHighway() {
	if (!highwayInAnimation) {
		startTime = GetTime();
		highwayInAnimation = true;
	}
	if (GetTime() <= startTime + animDuration && highwayInAnimation) {
		double timeSinceStart = GetTime() - startTime;
		highwayLevel = Remap(1.0 - getEasingFunction(EaseOutExpo)(timeSinceStart/animDuration), 0, 1.0, 0, -GetScreenHeight());
		highwayInEndAnim = true;
	}
};

void gameplayRenderer::LowerHighway() {
	if (!highwayOutAnimation) {
		startTime = GetTime();
		highwayOutAnimation = true;
	}
	if (GetTime() <= startTime + animDuration && highwayOutAnimation) {
		double timeSinceStart = GetTime() - startTime;
		highwayLevel = Remap(1.0 - getEasingFunction(EaseInExpo)(timeSinceStart/animDuration), 1.0, 0, 0, -GetScreenHeight());
		highwayOutEndAnim = true;
	}
};

void gameplayRenderer::RenderGameplay(Player* player, double time, Song song, RenderTexture2D& highway_tex, RenderTexture2D& hud_tex, RenderTexture2D& notes_tex, RenderTexture2D& highwayStatus_tex, RenderTexture2D& smasher_tex) {

	Chart& curChart = song.parts[player->Instrument]->charts[player->Difficulty];
	float highwayLength = defaultHighwayLength * gprSettings.highwayLengthMult;
	player->stats->Difficulty = player->Difficulty;
	player->stats->Instrument = player->Instrument;
	//float multFill = (!player->stats->Overdrive ? (float)(player->stats->multiplier() - 1) : (!player->stats->Multiplayer ? ((float)(player->stats->multiplier() / 2) - 1) / (float)player->stats->maxMultForMeter() : (float)(player->stats->multiplier() - 1)));
	float multFill = (!player->stats->Overdrive ? (float)(player->stats->multiplier() - 1) : ((float)(player->stats->multiplier() / 2) - 1)) / (float)player->stats->maxMultForMeter();

	//float multFill = 0.0;


	SetShaderValue(gprAssets.odMultShader, gprAssets.multLoc, &multFill, SHADER_UNIFORM_FLOAT);
	SetShaderValue(gprAssets.multNumberShader, gprAssets.uvOffsetXLoc, &player->stats->uvOffsetX, SHADER_UNIFORM_FLOAT);
	SetShaderValue(gprAssets.multNumberShader, gprAssets.uvOffsetYLoc, &player->stats->uvOffsetY, SHADER_UNIFORM_FLOAT);
	float comboFill = player->stats->comboFillCalc();
	int isBassOrVocal = 0;
	if (player->Instrument == 1 || player->Instrument == 3 || player->Instrument == 5) {
		isBassOrVocal = 1;
	}
	SetShaderValue(gprAssets.odMultShader, gprAssets.isBassOrVocalLoc, &isBassOrVocal, SHADER_UNIFORM_INT);
	SetShaderValue(gprAssets.odMultShader, gprAssets.comboCounterLoc, &comboFill, SHADER_UNIFORM_FLOAT);
	SetShaderValue(gprAssets.odMultShader, gprAssets.odLoc, &player->stats->overdriveFill, SHADER_UNIFORM_FLOAT);

	int PlayerComboMax = (player->Instrument == 1 || player->Instrument == 3 || player->Instrument == 5) ? 50 : 30;

	Color highwayColor = ColorContrast(accentColor, Clamp(Remap(player->stats->Combo, 0, PlayerComboMax, -0.6f, 0.0f), -0.6, 0.0f));

	gprAssets.expertHighway.materials[0].maps[MATERIAL_MAP_ALBEDO].color = highwayColor;
	gprAssets.emhHighway.materials[0].maps[MATERIAL_MAP_ALBEDO].color = highwayColor;
	gprAssets.smasherBoard.materials[0].maps[MATERIAL_MAP_ALBEDO].color = highwayColor;
	gprAssets.smasherBoardEMH.materials[0].maps[MATERIAL_MAP_ALBEDO].color = highwayColor;

	if (player->stats->Overdrive) gprAssets.expertHighwaySides.materials[0].maps[MATERIAL_MAP_ALBEDO].color = OverdriveColor;
	else if (!player->Bot) gprAssets.expertHighwaySides.materials[0].maps[MATERIAL_MAP_ALBEDO].color = accentColor;
	if (player->Bot) gprAssets.expertHighwaySides.materials[0].maps[MATERIAL_MAP_ALBEDO].color = GOLD;

	if (player->stats->Overdrive) gprAssets.emhHighwaySides.materials[0].maps[MATERIAL_MAP_ALBEDO].color = OverdriveColor;
	else if (!player->Bot) gprAssets.emhHighwaySides.materials[0].maps[MATERIAL_MAP_ALBEDO].color = accentColor;
	if (player->Bot) gprAssets.emhHighwaySides.materials[0].maps[MATERIAL_MAP_ALBEDO].color = GOLD;

	if (player->Bot) player->stats->FC = false;

	if (player->Bot) player->Bot = true;
	else player->Bot = false;
	gprAudioManager.BeginPlayback(gprAudioManager.loadedStreams[0].handle);
	RaiseHighway();
	if (GetTime() >= startTime + animDuration && highwayInEndAnim) {
		highwayInEndAnim = false;
	}

	/*
	if ((player->stats->Overdrive ? player.multiplier(player->Instrument) / 2 : player.multiplier(player->Instrument))>= (player->Instrument == 1 || player->Instrument == 3 || player->Instrument == 5 ? 6 : 4)) {
		gprAssets.expertHighway.materials[0].maps[MATERIAL_MAP_ALBEDO].color = accentColor;
		gprAssets.emhHighway.materials[0].maps[MATERIAL_MAP_ALBEDO].color = accentColor;
		gprAssets.smasherBoard.materials[0].maps[MATERIAL_MAP_ALBEDO].color = accentColor;
		gprAssets.smasherBoardEMH.materials[0].maps[MATERIAL_MAP_ALBEDO].color = accentColor;
	}
	else {
		gprAssets.expertHighway.materials[0].maps[MATERIAL_MAP_ALBEDO].color = GRAY;
		gprAssets.emhHighway.materials[0].maps[MATERIAL_MAP_ALBEDO].color = GRAY;
		gprAssets.smasherBoard.materials[0].maps[MATERIAL_MAP_ALBEDO].color = GRAY;
		gprAssets.smasherBoardEMH.materials[0].maps[MATERIAL_MAP_ALBEDO].color = GRAY;
	}
	 */


	double musicTime = gprAudioManager.GetMusicTimePlayed(gprAudioManager.loadedStreams[0].handle);
	if (player->stats->Overdrive) {

		// gprAssets.expertHighway.materials[0].maps[MATERIAL_MAP_ALBEDO].texture = gprAssets.highwayTextureOD;
		// gprAssets.emhHighway.materials[0].maps[MATERIAL_MAP_ALBEDO].texture = gprAssets.highwayTextureOD;
		if (!gprPlayerManager.BandStats.Multiplayer) {
			gprAssets.multBar.materials[0].maps[MATERIAL_MAP_EMISSION].texture = gprAssets.odMultFillActive;
			gprAssets.multCtr3.materials[0].maps[MATERIAL_MAP_EMISSION].texture = gprAssets.odMultFillActive;
			gprAssets.multCtr5.materials[0].maps[MATERIAL_MAP_EMISSION].texture = gprAssets.odMultFillActive;
		}
		// THIS IS LOGIC!
		player->stats->overdriveFill = player->stats->overdriveActiveFill - (float)((musicTime - player->stats->overdriveActiveTime) / (1920 / song.bpms[player->stats->curBPM].bpm));
		if (player->stats->overdriveFill <= 0) {
			player->stats->overdriveActivateTime = musicTime;
			player->stats->Overdrive = false;
			player->stats->overdriveActiveFill = 0;
			player->stats->overdriveActiveTime = 0.0;
			if (gprPlayerManager.BandStats.Multiplayer) {
				gprPlayerManager.BandStats.PlayersInOverdrive -= 1;
				gprPlayerManager.BandStats.Overdrive = false;;
			}

			gprAssets.expertHighway.materials[0].maps[MATERIAL_MAP_ALBEDO].texture = gprAssets.highwayTexture;
			gprAssets.emhHighway.materials[0].maps[MATERIAL_MAP_ALBEDO].texture = gprAssets.highwayTexture;
			gprAssets.multBar.materials[0].maps[MATERIAL_MAP_EMISSION].texture = gprAssets.odMultFill;
			gprAssets.multCtr3.materials[0].maps[MATERIAL_MAP_EMISSION].texture = gprAssets.odMultFill;
			gprAssets.multCtr5.materials[0].maps[MATERIAL_MAP_EMISSION].texture = gprAssets.odMultFill;

		}
	}

	for (int i = player->stats->curBPM; i < song.bpms.size(); i++) {
		if (musicTime > song.bpms[i].time && i < song.bpms.size() - 1)
			player->stats->curBPM++;
	}

	if (!curChart.odPhrases.empty() && player->stats->curODPhrase<curChart.odPhrases.size() - 1 && musicTime>curChart.odPhrases[player->stats->curODPhrase].end && (curChart.odPhrases[player->stats->curODPhrase].added ||curChart.odPhrases[player->stats->curODPhrase].missed)) {
		player->stats->curODPhrase++;
	}

	if (!curChart.Solos.empty() && player->stats->curSolo<curChart.Solos.size() - 1 && musicTime-2.5f>curChart.Solos[player->stats->curSolo].end) {
		player->stats->curSolo++;
	}

	if (player->Instrument == 4) {
		RenderPDrumsHighway(player, song, time, highway_tex, highwayStatus_tex, smasher_tex);
	}
	else if (player->Difficulty == 3 || (player->ClassicMode && player->Instrument!=4)) {
		RenderExpertHighway(player, song, time, highway_tex, highwayStatus_tex, smasher_tex);
	} else {
		RenderEmhHighway(player, song, time, highway_tex);
	}
	if (player->ClassicMode) {
		if (player->Instrument == 4) {
			RenderPDrumsNotes(player, curChart, time, notes_tex, highwayLength);
		}
		else {
			RenderClassicNotes(player, curChart, time, notes_tex, highwayLength);
		}
	} else {
		RenderNotes(player, curChart, time, notes_tex, highwayLength);
	}
	// if (!player->Bot)
		RenderHud(player, hud_tex, highwayLength);
}

void gameplayRenderer::RenderExpertHighway(Player* player, Song song, double time, RenderTexture2D& highway_tex, RenderTexture2D& highwayStatus_tex, RenderTexture2D& smasher_tex)  {
	BeginTextureMode(highway_tex);
	ClearBackground({0,0,0,0});
	BeginMode3D(camera3pVector[cameraSel]);

	float diffDistance = 2.0f;
	float lineDistance = 1.5f;

	float highwayLength = defaultHighwayLength * gprSettings.highwayLengthMult;
	float highwayPosShit = ((20) * (1 - gprSettings.highwayLengthMult));

	textureOffset += 0.1f;

	//DrawTriangle3D({-diffDistance-0.5f,-0.002,0},{-diffDistance-0.5f,-0.002,(highwayLength *1.5f) + smasherPos},{diffDistance+0.5f,-0.002,0},Color{0,0,0,255});
	//DrawTriangle3D({diffDistance+0.5f,-0.002,(highwayLength *1.5f) + smasherPos},{diffDistance+0.5f,-0.002,0},{-diffDistance-0.5f,-0.002,(highwayLength *1.5f) + smasherPos},Color{0,0,0,255});

	DrawModel(gprAssets.expertHighwaySides, Vector3{ 0,0,gprSettings.highwayLengthMult < 1.0f ? -(highwayPosShit* (0.875f)) : -0.2f }, 1.0f, WHITE);
	DrawModel(gprAssets.expertHighway, Vector3{ 0,0,gprSettings.highwayLengthMult < 1.0f ? -(highwayPosShit* (0.875f)) : -0.2f }, 1.0f, WHITE);
	if (gprSettings.highwayLengthMult > 1.0f) {
		DrawModel(gprAssets.expertHighway, Vector3{ 0,0,((highwayLength*1.5f)+smasherPos)-20-0.2f }, 1.0f, WHITE);
		DrawModel(gprAssets.expertHighwaySides, Vector3{ 0,0,((highwayLength*1.5f)+smasherPos)-20-0.2f }, 1.0f, WHITE);
		if (highwayLength > 23.0f) {
			DrawModel(gprAssets.expertHighway, Vector3{ 0,0,((highwayLength*1.5f)+smasherPos)-40-0.2f }, 1.0f, WHITE);
			DrawModel(gprAssets.expertHighwaySides, Vector3{ 0,0,((highwayLength*1.5f)+smasherPos)-40-0.2f }, 1.0f, WHITE);
		}
	}
	unsigned char laneColor = 0;
	unsigned char laneAlpha = 48;
	unsigned char OverdriveAlpha = 255;

	double OverdriveAnimDuration = 0.25f;

	if (gprAudioManager.GetMusicTimePlayed(gprAudioManager.loadedStreams[0].handle) <= player->stats->overdriveActiveTime + OverdriveAnimDuration) {
		double TimeSinceOverdriveActivate = gprAudioManager.GetMusicTimePlayed(gprAudioManager.loadedStreams[0].handle) - player->stats->overdriveActiveTime;
		OverdriveAlpha = Remap(getEasingFunction(EaseOutQuint)(TimeSinceOverdriveActivate/OverdriveAnimDuration), 0, 1.0, 0, 255);
	} else OverdriveAlpha = 255;

	if (gprAudioManager.GetMusicTimePlayed(gprAudioManager.loadedStreams[0].handle) <= player->stats->overdriveActivateTime + OverdriveAnimDuration && gprAudioManager.GetMusicTimePlayed(gprAudioManager.loadedStreams[0].handle) > 0.0) {
		double TimeSinceOverdriveActivate = gprAudioManager.GetMusicTimePlayed(gprAudioManager.loadedStreams[0].handle) - player->stats->overdriveActivateTime;
		OverdriveAlpha = Remap(getEasingFunction(EaseOutQuint)(TimeSinceOverdriveActivate/OverdriveAnimDuration), 0, 1.0, 255, 0);
	} else if (!player->stats->Overdrive) OverdriveAlpha = 0;

	if (player->stats->Overdrive || gprAudioManager.GetMusicTimePlayed(gprAudioManager.loadedStreams[0].handle) <= player->stats->overdriveActivateTime + OverdriveAnimDuration) {DrawModel(gprAssets.odHighwayX, Vector3{0,0.001f,0},1,Color{255,255,255,OverdriveAlpha});}
	BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
	DrawTriangle3D({lineDistance - 1.0f,0.003,0},
				   {lineDistance - 1.0f,0.003,(highwayLength *1.5f) + smasherPos},
				   {lineDistance,0.003,0},
				   Color{laneColor,laneColor,laneColor,laneAlpha});

	DrawTriangle3D({lineDistance,0.003,(highwayLength *1.5f) + smasherPos},
				   {lineDistance,0.003,0},
				   {lineDistance - 1.0f,0.003,(highwayLength *1.5f) + smasherPos},
				   Color{laneColor,laneColor,laneColor,laneAlpha});

	//for (int i = 0; i < 4; i++) {
	//    float radius = player->ClassicMode ? 0.02 : ((i == (gprSettings.mirrorMode ? 2 : 1)) ? 0.05 : 0.02);
//
	//    DrawCylinderEx(Vector3{ lineDistance - (float)i, 0, smasherPos + 0.5f }, Vector3{ lineDistance - i, 0, (highwayLength *1.5f) + smasherPos }, radius, radius, 15, Color{ 128,128,128,128 });
	//}

	DrawTriangle3D({0-lineDistance,0.003,0},
				   {0-lineDistance,0.003,(highwayLength *1.5f) + smasherPos},
				   {0-lineDistance + 1.0f,0.003,0},
				   Color{laneColor,laneColor,laneColor,laneAlpha});

	DrawTriangle3D({0-lineDistance + 1.0f,0.003,(highwayLength *1.5f) + smasherPos},
				   {0-lineDistance + 1.0f,0.003,0},
				   {0-lineDistance,0.003,(highwayLength *1.5f) + smasherPos},
				   Color{laneColor,laneColor,laneColor,laneAlpha});

	if (!player->ClassicMode)
		DrawCylinderEx(Vector3{ lineDistance-1.0f, 0, smasherPos}, Vector3{ lineDistance-1.0f, 0, (highwayLength *1.5f) + smasherPos }, 0.025f, 0.025f, 15, Color{ 128,128,128,128 });

	EndBlendMode();

	EndMode3D();
	EndTextureMode();

	SetTextureWrap(highway_tex.texture,TEXTURE_WRAP_CLAMP);
	highway_tex.texture.width = (float)GetScreenWidth();
	highway_tex.texture.height = (float)GetScreenHeight();
	DrawTexturePro(highway_tex.texture, {0,0,(float)GetScreenWidth(), (float)-GetScreenHeight() },{ 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight() }, {renderPos,highwayLevel}, 0, WHITE );

	BeginBlendMode(BLEND_ALPHA);
	BeginTextureMode(highwayStatus_tex);
	ClearBackground({0,0,0,0});
	BeginMode3D(camera3pVector[cameraSel]);

	if (!song.beatLines.empty()) {
		DrawBeatlines(player, song, highwayLength, time);
	}

	if (!song.parts[player->Instrument]->charts[player->Difficulty].odPhrases.empty()) {
		DrawOverdrive(player, song.parts[player->Instrument]->charts[player->Difficulty], highwayLength, time);
	}
	if (!song.parts[player->Instrument]->charts[player->Difficulty].Solos.empty()) {
		DrawSolo(player, song.parts[player->Instrument]->charts[player->Difficulty], highwayLength, time);
	}

	float darkYPos = 0.015f;

	BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
	DrawTriangle3D({-diffDistance-0.5f,darkYPos,smasherPos},{-diffDistance-0.5f,darkYPos,(highwayLength *1.5f) + smasherPos},{diffDistance+0.5f,darkYPos,smasherPos},Color{0,0,0,64});
	DrawTriangle3D({diffDistance+0.5f,darkYPos,(highwayLength *1.5f) + smasherPos},{diffDistance+0.5f,darkYPos,smasherPos},{-diffDistance-0.5f,darkYPos,(highwayLength *1.5f) + smasherPos},Color{0,0,0,64});



	EndBlendMode();
	EndMode3D();
	EndTextureMode();
	SetTextureWrap(highwayStatus_tex.texture,TEXTURE_WRAP_CLAMP);
	highwayStatus_tex.texture.width = (float)GetScreenWidth();
	highwayStatus_tex.texture.height = (float)GetScreenHeight();
	DrawTexturePro(highwayStatus_tex.texture, {0,0,(float)GetScreenWidth(), (float)-GetScreenHeight() },{ 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight() }, {renderPos,highwayLevel}, 0, WHITE );

	BeginTextureMode(smasher_tex);
	ClearBackground({0,0,0,0});
	BeginMode3D(camera3pVector[cameraSel]);
	BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
	DrawModel(gprAssets.smasherBoard, Vector3{ 0, 0.004f, 0 }, 1.0f, WHITE);
	BeginBlendMode(BLEND_ALPHA);
	for (int i = 0; i < 5;  i++) {
		Color NoteColor; // = gprMenu.hehe && player->Difficulty == 3 ? i == 0 || i == 4 ? SKYBLUE : i == 1 || i == 3 ? PINK : WHITE : accentColor;
		int noteColor = gprSettings.mirrorMode ? 4 - i : i;
		if (player->ClassicMode) {
			bool pd = (player->Instrument == 4);

			switch (noteColor) {
				case 0:
					NoteColor = gprMenu.hehe ? SKYBLUE : GREEN;
					break;
				case 1:
					NoteColor = gprMenu.hehe ? PINK :RED;
					break;
				case 2:
					NoteColor = gprMenu.hehe ? RAYWHITE :YELLOW;
					break;
				case 3:
					NoteColor = gprMenu.hehe ? PINK :BLUE;
					break;
				case 4:
					NoteColor = gprMenu.hehe ? SKYBLUE :ORANGE;
					break;
				default:
					NoteColor = accentColor;
					break;
			}
		}   else {
			NoteColor = gprMenu.hehe ? (i == 0 || i == 4 ? SKYBLUE : (i == 1 || i == 3 ? PINK : WHITE)) : accentColor;
		}

		gprAssets.smasherPressed.materials[0].maps[MATERIAL_MAP_ALBEDO].color = NoteColor;
		gprAssets.smasherReg.materials[0].maps[MATERIAL_MAP_ALBEDO].color = NoteColor;

		if (player->stats->HeldFrets[noteColor] || player->stats->HeldFretsAlt[noteColor]) {
			DrawModel(gprAssets.smasherPressed, Vector3{ diffDistance - (float)(i), 0.01f, smasherPos }, 1.0f, WHITE);
		}
		else {
			DrawModel(gprAssets.smasherReg, Vector3{ diffDistance - (float)(i), 0.01f, smasherPos }, 1.0f, WHITE);
		}
	}
	//DrawModel(gprAssets.lanes, Vector3 {0,0.1f,0}, 1.0f, WHITE);

	EndBlendMode();
	EndMode3D();
	EndTextureMode();
	SetTextureWrap(smasher_tex.texture,TEXTURE_WRAP_CLAMP);
	smasher_tex.texture.width = (float)GetScreenWidth();
	smasher_tex.texture.height = (float)GetScreenHeight();
	DrawTexturePro(smasher_tex.texture, {0,0,(float)GetScreenWidth(), (float)-GetScreenHeight() },{ 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight() }, {renderPos,highwayLevel}, 0, WHITE );



}

void gameplayRenderer::RenderEmhHighway(Player* player, Song song, double time, RenderTexture2D &highway_tex) {
	BeginTextureMode(highway_tex);
	ClearBackground({0,0,0,0});
	BeginMode3D(camera3pVector[cameraSel]);

	float diffDistance = player->Difficulty == 3 ? 2.0f : 1.5f;
	float lineDistance = player->Difficulty == 3 ? 1.5f : 1.0f;

	float highwayLength = defaultHighwayLength * gprSettings.highwayLengthMult;
	float highwayPosShit = ((20) * (1 - gprSettings.highwayLengthMult));

	DrawModel(gprAssets.emhHighwaySides, Vector3{ 0,0,gprSettings.highwayLengthMult < 1.0f ? -(highwayPosShit* (0.875f)) : 0 }, 1.0f, WHITE);
	DrawModel(gprAssets.emhHighway, Vector3{ 0,0,gprSettings.highwayLengthMult < 1.0f ? -(highwayPosShit* (0.875f)) : 0 }, 1.0f, WHITE);
	if (gprSettings.highwayLengthMult > 1.0f) {
		DrawModel(gprAssets.emhHighway, Vector3{ 0,0,((highwayLength*1.5f)+smasherPos)-20 }, 1.0f, WHITE);
		DrawModel(gprAssets.emhHighwaySides, Vector3{ 0,0,((highwayLength*1.5f)+smasherPos)-20 }, 1.0f, WHITE);
		if (highwayLength > 23.0f) {
			DrawModel(gprAssets.emhHighway, Vector3{ 0,0,((highwayLength*1.5f)+smasherPos)-40 }, 1.0f, WHITE);
			DrawModel(gprAssets.emhHighwaySides, Vector3{ 0,0,((highwayLength*1.5f)+smasherPos)-40 }, 1.0f, WHITE);
		}
	}
	if (player->stats->Overdrive) {DrawModel(gprAssets.odHighwayEMH, Vector3{0,0.001f,0},1,WHITE);}

	DrawTriangle3D({-diffDistance-0.5f,0.002,smasherPos},{-diffDistance-0.5f,0.002,(highwayLength *1.5f) + smasherPos},{diffDistance+0.5f,0.002,smasherPos},Color{0,0,0,64});
	DrawTriangle3D({diffDistance+0.5f,0.002,(highwayLength *1.5f) + smasherPos},{diffDistance+0.5f,0.002,smasherPos},{-diffDistance-0.5f,0.002,(highwayLength *1.5f) + smasherPos},Color{0,0,0,64});

	DrawModel(gprAssets.smasherBoardEMH, Vector3{ 0, 0.001f, 0 }, 1.0f, WHITE);

	for (int i = 0; i < 4; i++) {
		Color NoteColor = gprMenu.hehe && player->Difficulty == 3 ? i == 0 || i == 4 ? SKYBLUE : i == 1 || i == 3 ? PINK : WHITE : accentColor;

		gprAssets.smasherPressed.materials[0].maps[MATERIAL_MAP_ALBEDO].color = NoteColor;
		gprAssets.smasherReg.materials[0].maps[MATERIAL_MAP_ALBEDO].color = NoteColor;

		if (player->stats->HeldFrets[i] || player->stats->HeldFretsAlt[i]) {
			DrawModel(gprAssets.smasherPressed, Vector3{ diffDistance - (float)(i), 0.01f, smasherPos }, 1.0f, WHITE);
		}
		else {
			DrawModel(gprAssets.smasherReg, Vector3{ diffDistance - (float)(i), 0.01f, smasherPos }, 1.0f, WHITE);

		}
	}
	for (int i = 0; i < 3; i++) {
		float radius = (i == 1) ? 0.03 : 0.01;
		DrawCylinderEx(Vector3{ lineDistance - (float)i, 0, smasherPos + 0.5f }, Vector3{ lineDistance - (float)i, 0, (highwayLength *1.5f) + smasherPos }, radius,
					   radius, 4.0f, Color{ 128, 128, 128, 128 });
	}
	if (!song.beatLines.empty()) {
		DrawBeatlines(player, song, highwayLength, time);
	}
	if (!song.parts[player->Instrument]->charts[player->Difficulty].Solos.empty()) {
		DrawSolo(player, song.parts[player->Instrument]->charts[player->Difficulty], highwayLength, time);
	}
	if (!song.parts[player->Instrument]->charts[player->Difficulty].odPhrases.empty()) {
		DrawOverdrive(player, song.parts[player->Instrument]->charts[player->Difficulty], highwayLength, time);
	}

	EndBlendMode();
	EndMode3D();
	EndTextureMode();

	SetTextureWrap(highway_tex.texture,TEXTURE_WRAP_CLAMP);
	highway_tex.texture.width = (float)GetScreenWidth();
	highway_tex.texture.height = (float)GetScreenHeight();
	DrawTexturePro(highway_tex.texture, {0,0,(float)GetScreenWidth(), (float)-GetScreenHeight() },{ 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight() }, {0,highwayLevel}, 0, WHITE );
}

void gameplayRenderer::DrawBeatlines(Player* player, Song song, float length, double musicTime) {
	float DiffMultiplier = Remap(player->Difficulty, 0, 3, 1.0f, 1.75f);
	float diffDistance = player->Difficulty == 3 || player->ClassicMode ? 2.0f : 1.5f;
	float lineDistance = player->Difficulty == 3 || player->ClassicMode ? 1.5f : 1.0f;
	std::vector<std::pair<double, bool>> beatlines = song.beatLines;

	if (beatlines.size() >= 0) {
		for (int i = player->stats->curBeatLine; i < beatlines.size(); i++) {
			if (beatlines[i].first >= song.music_start-1 && beatlines[i].first <= song.end) {
				Color BeatLineColor = {255,255,255,128};
				double relTime = ((song.beatLines[i].first - musicTime)) * (player->NoteSpeed * DiffMultiplier) * ( 11.5f / length);
				if (i > 0) {
					double secondLine = ((((song.beatLines[i-1].first + song.beatLines[i].first)/2) - musicTime)) * (player->NoteSpeed * DiffMultiplier)  * ( 11.5f / length);
					if (secondLine > 1.5) break;

					if (song.beatLines[i].first - song.beatLines[i-1].first <= 0.2 || (smasherPos + (length * (float)relTime)) - (smasherPos + (length * (float)secondLine)) <= 1.5) BeatLineColor = {0,0,0,0};
					else BeatLineColor = {128,128,128,128};
					DrawCylinderEx(Vector3{ -diffDistance - 0.5f,0,smasherPos + (length * (float)secondLine) },
								   Vector3{ diffDistance + 0.5f,0,smasherPos + (length * (float)secondLine) },
								   0.01f,
								   0.01f,
								   4,
								   BeatLineColor);
				}

				if (relTime > 1.5) break;
				float radius = beatlines[i].second ? 0.06f : 0.03f;

				BeatLineColor = (beatlines[i].second) ? Color{ 255, 255, 255, 196 } : Color{ 255, 255, 255, 128 };

				DrawCylinderEx(Vector3{ -diffDistance - 0.5f,0,smasherPos + (length * (float)relTime) },
							   Vector3{ diffDistance + 0.5f,0,smasherPos + (length * (float)relTime) },
							   radius,
							   radius,
							   4,
							   BeatLineColor);


				if (relTime < -1 && player->stats->curBeatLine < beatlines.size() - 1) {
					player->stats->curBeatLine++;
				}
			}
		}
	}
}

void gameplayRenderer::DrawOverdrive(Player* player,  Chart& curChart, float length, double musicTime) {
	float DiffMultiplier = Remap(player->Difficulty, 0, 3, 1.0f, 1.75f);
	float odStart = (float)((curChart.odPhrases[player->stats->curODPhrase].start - musicTime)) * (player->NoteSpeed * DiffMultiplier) * (11.5f / length);
	float odEnd = (float)((curChart.odPhrases[player->stats->curODPhrase].end - musicTime)) * (player->NoteSpeed * DiffMultiplier) * (11.5f / length);

	// can be flipped. btw

	// horrifying.
	// main calc
	bool Beginning = (float)(smasherPos + (length * odStart)) >= (length * 1.5f) + smasherPos;
	bool Ending = (float)(smasherPos + (length * odEnd)) >= (length * 1.5f) + smasherPos;

	float HighwayEnd = (length * 1.5f) + smasherPos;

	// right calc
	float RightSideX = player->Difficulty == 3 || player->ClassicMode ? 2.7f : 2.2f;

	Vector3 RightSideStart = {RightSideX ,0,Beginning ?  HighwayEnd : (float)(smasherPos + (length * odStart)) };
	Vector3 RightSideEnd = { RightSideX,0, Ending ? HighwayEnd : (float)(smasherPos + (length * odEnd)) };

	// left calc
	float LeftSideX = player->Difficulty == 3 || player->ClassicMode ? -2.7f : -2.2f;

	Vector3 LeftSideStart = {LeftSideX ,0,Beginning ?  HighwayEnd : (float)(smasherPos + (length * odStart)) };
	Vector3 LeftSideEnd = { LeftSideX,0, Ending ? HighwayEnd : (float)(smasherPos + (length * odEnd)) };

	// draw
	DrawCylinderEx(RightSideStart, RightSideEnd, 0.07, 0.07, 10, RAYWHITE);
	DrawCylinderEx(LeftSideStart, LeftSideEnd, 0.07, 0.07, 10, RAYWHITE);
}

void gameplayRenderer::DrawSolo(Player* player, Chart& curChart, float length, double musicTime) {
	float DiffMultiplier = Remap(player->Difficulty, 0, 3, 1.0f, 1.75f);
	float soloStart = (float)((curChart.Solos[player->stats->curSolo].start - musicTime)) * (player->NoteSpeed * DiffMultiplier) * (11.5f / length);
	float soloEnd = (float)((curChart.Solos[player->stats->curSolo].end - musicTime)) * (player->NoteSpeed * DiffMultiplier) * (11.5f / length);

	if (!curChart.Solos.empty() && musicTime >= curChart.Solos[player->stats->curSolo].start - 1 && musicTime <= curChart.Solos[player->stats->curSolo].end + 2.5) {
		int solopctnum = Remap(curChart.Solos[player->stats->curSolo].notesHit, 0, curChart.Solos[player->stats->curSolo].noteCount, 0, 100);
		Color accColor = solopctnum == 100 ? GOLD : WHITE;
		const char* soloPct = TextFormat("%i%%", solopctnum);
		float soloPercentLength = MeasureTextEx(gprAssets.rubikBold, soloPct, gprU.hinpct(0.09f), 0).x;

		Vector2 SoloBoxPos = {(GetScreenWidth()/2) - (soloPercentLength/2), gprU.hpct(0.2f)};

		//DrawTextEx(gprAssets.rubikBold, soloPct, SoloBoxPos, gprU.hinpct(0.09f), 0, accColor);

		const char* soloHit = TextFormat("%i/%i", curChart.Solos[player->stats->curSolo].notesHit, curChart.Solos[player->stats->curSolo].noteCount);
		float soloHitLength = MeasureTextEx(gprAssets.josefinSansItalic, soloHit, gprU.hinpct(0.04f), 0).x;

		Vector2 SoloHitPos = {(GetScreenWidth()/2) - (soloHitLength/2), gprU.hpct(0.2f) + gprU.hinpct(0.1f)};

		//DrawTextEx(gprAssets.josefinSansItalic, soloHit, SoloHitPos, gprU.hinpct(0.04f), 0, accColor);
		sol::state lua;
		lua.script_file("scripts/ui/solo.lua");

		float posY =  lua["posY"];

		float height = lua["height"];
		float pctDist = lua["pctDist"];
		float praiseDist = lua["praiseDist"];

		float fontSize = lua["fontSize"];
		float fontSizee = lua["fontSizee"];

		rlPushMatrix();
			rlRotatef(180, 0, 1, 0);
			rlRotatef(90, 1, 0, 0);
			//rlRotatef(90.0f, 0.0f, 0.0f, 1.0f);								//0, 1, 2.4
			float soloWidth = MeasureText3D(gprAssets.rubikBold, soloPct, fontSizee, 0, 0).x/2;
			float hitWidth = MeasureText3D(gprAssets.josefinSansItalic, soloHit, fontSize, 0, 0).x/2;
			DrawText3D(gprAssets.rubikBold, soloPct, Vector3{ 0-soloWidth,posY,height-pctDist }, fontSizee, 0, 0, 1, accColor);
			if (musicTime <= curChart.Solos[player->stats->curSolo].end)
				DrawText3D(gprAssets.josefinSansItalic, soloHit, Vector3{ 0-hitWidth,posY,height }, fontSize, 0, 0, 1, accColor);
		rlPopMatrix();
		if (musicTime >= curChart.Solos[player->stats->curSolo].end && musicTime <= curChart.Solos[player->stats->curSolo].end + 2.5) {

			const char* PraiseText = "";
			if (solopctnum == 100) {
				PraiseText = "Perfect Solo!";
			} else if (solopctnum == 99) {
				PraiseText  = "Awesome Choke!";
			} else if (solopctnum > 90) {
				PraiseText  = "Awesome solo!";
			} else if (solopctnum > 80) {
				PraiseText  = "Great solo!";
			} else if (solopctnum > 75) {
				PraiseText  = "Decent solo";
			} else if (solopctnum > 50) {
				PraiseText  = "OK solo";
			} else if (solopctnum > 0) {
				PraiseText  = "Bad solo";
			}
			rlPushMatrix();
				rlRotatef(180, 0, 1, 0);
				rlRotatef(90, 1, 0, 0);
				//rlRotatef(90.0f, 0.0f, 0.0f, 1.0f);								//0, 1, 2.4
				float praiseWidth = MeasureText3D(gprAssets.josefinSansItalic, PraiseText, fontSize, 0, 0).x/2;
				DrawText3D(gprAssets.josefinSansItalic, PraiseText, Vector3{ 0-praiseWidth,posY,height-praiseDist }, fontSize, 0, 0, 1, accColor);
			rlPopMatrix();
			// DrawTextEx(gprAssets.josefinSansItalic, PraiseText, PraisePos, gprU.hinpct(0.05f), 0, accColor);
		}
	}

	// can be flipped. btw

	// horrifying.
	// main calc
	bool Beginning = (float)(smasherPos + (length * soloStart)) >= (length * 1.5f) + smasherPos;
	bool Ending = (float)(smasherPos + (length * soloEnd)) >= (length * 1.5f) + smasherPos;
	float HighwayEnd = (length * 1.5f) + smasherPos;

	float soloPlaneStart = Beginning ?  HighwayEnd : (float)(smasherPos + (length * soloStart));
	float soloPlaneEnd = Ending ? HighwayEnd : (float)(smasherPos + (length * soloEnd));

	//float soloLen = (length * (float) soloPlaneEnd) - (length * (float) soloPlaneStart);
	// Matrix soloMatrix = MatrixMultiply(MatrixScale(1, 1, soloLen),
	//                                   MatrixTranslate(0, 0.005f,
//                                                       (length *
	//                                                    (float) soloStart) +
	//                                                   (soloLen / 2.0f)));
	//gprAssets.soloMat.maps[MATERIAL_MAP_DIFFUSE].color = SKYBLUE;

	// DrawMesh(soloPlane, gprAssets.soloMat, soloMatrix);


	// right calc
	float RightSideX = player->Difficulty == 3 || player->ClassicMode ? 2.7f : 2.2f;

	Vector3 RightSideStart = {RightSideX ,-0.0025,Beginning ?  HighwayEnd : (float)(smasherPos + (length * soloStart)) };
	Vector3 RightSideEnd = { RightSideX,-0.0025, Ending ? HighwayEnd : (float)(smasherPos + (length * soloEnd)) };

	// left calc
	float LeftSideX = player->Difficulty == 3 || player->ClassicMode ? -2.7f : -2.2f;

	Vector3 LeftSideStart = {LeftSideX ,-0.0025,Beginning ?  HighwayEnd : (float)(smasherPos + (length * soloStart)) };
	Vector3 LeftSideEnd = { LeftSideX,-0.0025, Ending ? HighwayEnd : (float)(smasherPos + (length * soloEnd)) };

	// draw
	DrawCylinderEx(RightSideStart, RightSideEnd, 0.07, 0.07, 10, SKYBLUE);
	DrawCylinderEx(LeftSideStart, LeftSideEnd, 0.07, 0.07, 10, SKYBLUE);
}

// classic drums
enum DrumNotes {
	KICK,
	RED_DRUM,
	YELLOW_DRUM,
	BLUE_DRUM,
	GREEN_DRUM
};

void gameplayRenderer::RenderPDrumsNotes(Player* player, Chart& curChart, double time, RenderTexture2D& notes_tex, float length) {
	float diffDistance = 2.0f;
	float lineDistance = 1.0f;
	float DiffMultiplier = Remap(player->Difficulty, 0, 3, 1.0f, 1.75f);
	BeginTextureMode(notes_tex);
	ClearBackground({ 0,0,0,0 });
	BeginMode3D(camera3pVector[cameraSel]);
	// glDisable(GL_CULL_FACE);
	PlayerGameplayStats *stats = player->stats;

	for (auto& curNote : curChart.notes) {

		if (!curChart.Solos.empty()) {
			if (curNote.time >= curChart.Solos[stats->curSolo].start &&
				curNote.time < curChart.Solos[stats->curSolo].end) {
				if (curNote.hit) {
					if (curNote.hit && !curNote.countedForSolo) {
						curChart.Solos[stats->curSolo].notesHit++;
						curNote.countedForSolo = true;
					}
				}
			}
		}
		if (!curChart.odPhrases.empty()) {

			if (curNote.time >= curChart.odPhrases[stats->curODPhrase].start &&
				curNote.time < curChart.odPhrases[stats->curODPhrase].end &&
				!curChart.odPhrases[stats->curODPhrase].missed) {
				if (curNote.hit) {
					if (curNote.hit && !curNote.countedForODPhrase) {
						curChart.odPhrases[stats->curODPhrase].notesHit++;
						curNote.countedForODPhrase = true;
					}
				}
				curNote.renderAsOD = true;

			}
			if (curChart.odPhrases[stats->curODPhrase].missed) {
				curNote.renderAsOD = false;
			}
			if (curChart.odPhrases[stats->curODPhrase].notesHit ==
				curChart.odPhrases[stats->curODPhrase].noteCount &&
				!curChart.odPhrases[stats->curODPhrase].added && stats->overdriveFill < 1.0f) {
				stats->overdriveFill += 0.25f;
				if (stats->overdriveFill > 1.0f) stats->overdriveFill = 1.0f;
				if (stats->Overdrive) {
					stats->overdriveActiveFill = stats->overdriveFill;
					stats->overdriveActiveTime = time;
				}
				curChart.odPhrases[stats->curODPhrase].added = true;
			}
		}
		if (!curNote.hit && !curNote.accounted && curNote.time + goodBackend + player->InputCalibration < time &&
			!songEnded && stats->curNoteInt < curChart.notes.size() && !songEnded && !player->Bot) {
			TraceLog(LOG_INFO, TextFormat("Missed note at %f, note %01i", time, stats->curNoteInt));
			curNote.miss = true;
			FAS = false;
			stats->MissNote();
			if (!curChart.odPhrases.empty() && !curChart.odPhrases[stats->curODPhrase].missed &&
				curNote.time >= curChart.odPhrases[stats->curODPhrase].start &&
				curNote.time < curChart.odPhrases[stats->curODPhrase].end)
				curChart.odPhrases[stats->curODPhrase].missed = true;
			stats->Combo = 0;
			curNote.accounted = true;
			stats->curNoteInt++;
		}
		else if (player->Bot) {
			if (!curNote.hit && !curNote.accounted && curNote.time < time && stats->curNoteInt < curChart.notes.size() && !songEnded) {
				curNote.hit = true;
				player->stats->HitDrumsNote(false, !curNote.pDrumTom);
				if (gprPlayerManager.BandStats.Multiplayer) {
					gprPlayerManager.BandStats.DrumNotePoint(false, player->stats->noODmultiplier(), !curNote.pDrumTom);
				}
				if (curNote.len > 0) curNote.held = true;
				curNote.accounted = true;
				// stats->Combo++;
				curNote.accounted = true;
				curNote.hitTime = time;
				stats->curNoteInt++;
			}
		}

		double relTime = ((curNote.time - time)) *
			(player->NoteSpeed * DiffMultiplier) * (11.5f / length);

		Color NoteColor;
		switch (curNote.lane) {
		case 0:
			NoteColor = gprMenu.hehe ? SKYBLUE : ORANGE;
			break;
		case 1:
			NoteColor = gprMenu.hehe ? PINK : RED;
			break;
		case 2:
			NoteColor = gprMenu.hehe ? RAYWHITE : YELLOW;
			break;
		case 3:
			NoteColor = gprMenu.hehe ? PINK : BLUE;
			break;
		case 4:
			NoteColor = gprMenu.hehe ? SKYBLUE : GREEN;
			break;
		default:
			NoteColor = accentColor;
			break;
		}

		gprAssets.noteTopModel.materials[0].maps[MATERIAL_MAP_ALBEDO].color = NoteColor;
		gprAssets.noteTopModelHP.materials[0].maps[MATERIAL_MAP_ALBEDO].color = NoteColor;

		float notePosX = curNote.lane == KICK ? 0 : (diffDistance - (1.25f * (curNote.lane - 1)))-0.125f;
		float notePosY = curNote.lane == KICK ? 0.01f : 0;
		if (relTime > 1.5) {
			break;
		}
		Vector3 NoteScale = { 1.35f,1.35f,1.35f };
		if (curNote.lane == KICK) {
			NoteScale.x = 6.5f;
			NoteScale.y = 0.6f;
			NoteScale.z = 0.75f;
		}
		else if(curNote.lane>RED_DRUM && player->ProDrums) {
			if (!curNote.pDrumTom) {
				NoteScale.x = 0.75f;
			}
		}
		if (!curNote.hit && !curNote.miss) {
			if (curNote.renderAsOD) {
				DrawModelEx(gprAssets.noteTopModelOD, Vector3{ notePosX, notePosY, smasherPos +
																			(length *
																			(float)relTime) },Vector3{0,0,0},0, NoteScale,
					WHITE);
				DrawModelEx(gprAssets.noteBottomModelOD, Vector3{ notePosX, notePosY, smasherPos +
																			(length *
																				(float)relTime) }, Vector3{ 0,0,0 },0, NoteScale,
					WHITE);
			}
			else {
				DrawModelEx(gprAssets.noteTopModel, Vector3{ notePosX, notePosY, smasherPos +
																	   (length *
																		(float)relTime) }, Vector3{ 0,0,0 }, 0, NoteScale,
					WHITE);
				DrawModelEx(gprAssets.noteBottomModel, Vector3{ notePosX, notePosY, smasherPos +
																		  (length *
																		   (float)relTime) }, Vector3{ 0,0,0 }, 0, NoteScale,
					WHITE);
			}
		}
		else if (curNote.miss) {
			gprAssets.noteTopModel.materials[0].maps[MATERIAL_MAP_ALBEDO].color = RED;
			DrawModelEx(gprAssets.noteTopModel, Vector3{ notePosX, notePosY, smasherPos +
																		   (length *
																			(float)relTime) }, Vector3{ 0,0,0 }, 0, NoteScale,
				WHITE);
			DrawModelEx(gprAssets.noteBottomModel, Vector3{ notePosX, notePosY, smasherPos +
																	  (length *
																	   (float)relTime) }, Vector3{ 0,0,0 }, 0, NoteScale,
				WHITE);
		}

		if (curNote.miss && curNote.time + 0.5 < time) {
			if (gprAudioManager.GetMusicTimePlayed(gprAudioManager.loadedStreams[0].handle) <
				curNote.time + 0.4 && gprSettings.missHighwayColor) {
				gprAssets.expertHighway.materials[0].maps[MATERIAL_MAP_ALBEDO].color = RED;
			}
			else {
				gprAssets.expertHighway.materials[0].maps[MATERIAL_MAP_ALBEDO].color = accentColor;
			}
		}
		double PerfectHitAnimDuration = 1.0f;
		if (curNote.hit && gprAudioManager.GetMusicTimePlayed(gprAudioManager.loadedStreams[0].handle) <
			curNote.hitTime + HitAnimDuration) {

			double TimeSinceHit = gprAudioManager.GetMusicTimePlayed(gprAudioManager.loadedStreams[0].handle) - curNote.hitTime;
			unsigned char HitAlpha = Remap(getEasingFunction(EaseInBack)(TimeSinceHit / HitAnimDuration), 0, 1.0, 196, 0);

			DrawCube(Vector3{ notePosX, curNote.lane == KICK ? 0 : 0.125f, smasherPos }, curNote.lane == KICK ? 5.0f: 1.3f, curNote.lane == KICK ? 0.125f : 0.25f, curNote.lane == KICK ? 0.5f : 0.75f,
				curNote.perfect ? Color{ 255, 215, 0, HitAlpha } : Color{ 255, 255, 255, HitAlpha });
			// if (curNote.perfect) {
			// 	DrawCube(Vector3{3.3f, 0, player.smasherPos}, 1.0f, 0.01f,
			// 			 0.5f, Color{255,161,0,HitAlpha});
			// }


		}
		EndBlendMode();
		float KickBounceDuration = 0.5f;
		if (curNote.hit && gprAudioManager.GetMusicTimePlayed(gprAudioManager.loadedStreams[0].handle) <
			curNote.hitTime + PerfectHitAnimDuration && curNote.perfect) {

			double TimeSinceHit = gprAudioManager.GetMusicTimePlayed(gprAudioManager.loadedStreams[0].handle) - curNote.hitTime;
			unsigned char HitAlpha = Remap(getEasingFunction(EaseOutQuad)(TimeSinceHit / PerfectHitAnimDuration), 0, 1.0, 255, 0);
			float HitPosLeft = Remap(getEasingFunction(EaseInOutBack)(TimeSinceHit / PerfectHitAnimDuration), 0, 1.0, 3.4, 3.0);

			DrawCube(Vector3{ HitPosLeft, -0.1f, smasherPos }, 1.0f, 0.01f,
				0.5f, Color{ 255,161,0,HitAlpha });
			DrawCube(Vector3{ HitPosLeft, -0.11f, smasherPos }, 1.0f, 0.01f,
				1.0f, Color{ 255,161,0,(unsigned char)(HitAlpha / 2) });
		}
		if (curNote.hit && curNote.lane == KICK && gprAudioManager.GetMusicTimePlayed(gprAudioManager.loadedStreams[0].handle) <
			curNote.hitTime + KickBounceDuration) {
			double TimeSinceHit = gprAudioManager.GetMusicTimePlayed(gprAudioManager.loadedStreams[0].handle) - curNote.hitTime;
			float CameraPos = Remap(getEasingFunction(EaseOutBounce)(TimeSinceHit / KickBounceDuration), 0, 1.0, 7.75f, 8.0f);
			camera3pVector[cameraSel].position.y = CameraPos;

			//	float renderheight = 8.0f;
		}
	}
	EndMode3D();
	EndTextureMode();

	SetTextureWrap(notes_tex.texture, TEXTURE_WRAP_CLAMP);
	notes_tex.texture.width = (float)GetScreenWidth();
	notes_tex.texture.height = (float)GetScreenHeight();
	DrawTexturePro(notes_tex.texture, { 0,0,(float)GetScreenWidth(), (float)-GetScreenHeight() }, { 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight() }, { renderPos,highwayLevel }, 0, WHITE);
}

void gameplayRenderer::RenderPDrumsHighway(Player* player, Song song, double time, RenderTexture2D& highway_tex, RenderTexture2D& highwayStatus_tex, RenderTexture2D& smasher_tex) {
	BeginTextureMode(highway_tex);
	ClearBackground({0,0,0,0});
	BeginMode3D(camera3pVector[cameraSel]);
	PlayerGameplayStats *stats = player->stats;

	float diffDistance = 2.0f;
	float lineDistance = 1.5f;

	float highwayLength = defaultHighwayLength * gprSettings.highwayLengthMult;
	float highwayPosShit = ((20) * (1 - gprSettings.highwayLengthMult));

	textureOffset += 0.1f;

	//DrawTriangle3D({-diffDistance-0.5f,-0.002,0},{-diffDistance-0.5f,-0.002,(highwayLength *1.5f) + smasherPos},{diffDistance+0.5f,-0.002,0},Color{0,0,0,255});
	//DrawTriangle3D({diffDistance+0.5f,-0.002,(highwayLength *1.5f) + smasherPos},{diffDistance+0.5f,-0.002,0},{-diffDistance-0.5f,-0.002,(highwayLength *1.5f) + smasherPos},Color{0,0,0,255});

	DrawModel(gprAssets.expertHighwaySides, Vector3{ 0,0,gprSettings.highwayLengthMult < 1.0f ? -(highwayPosShit* (0.875f)) : -0.2f }, 1.0f, WHITE);
	DrawModel(gprAssets.expertHighway, Vector3{ 0,0,gprSettings.highwayLengthMult < 1.0f ? -(highwayPosShit* (0.875f)) : -0.2f }, 1.0f, WHITE);
	if (gprSettings.highwayLengthMult > 1.0f) {
		DrawModel(gprAssets.expertHighway, Vector3{ 0,0,((highwayLength*1.5f)+smasherPos)-20-0.2f }, 1.0f, WHITE);
		DrawModel(gprAssets.expertHighwaySides, Vector3{ 0,0,((highwayLength*1.5f)+smasherPos)-20-0.2f }, 1.0f, WHITE);
		if (highwayLength > 23.0f) {
			DrawModel(gprAssets.expertHighway, Vector3{ 0,0,((highwayLength*1.5f)+smasherPos)-40-0.2f }, 1.0f, WHITE);
			DrawModel(gprAssets.expertHighwaySides, Vector3{ 0,0,((highwayLength*1.5f)+smasherPos)-40-0.2f }, 1.0f, WHITE);
		}
	}
	unsigned char laneColor = 0;
	unsigned char laneAlpha = 48;
	unsigned char OverdriveAlpha = 255;

	double OverdriveAnimDuration = 0.25f;

	if (gprAudioManager.GetMusicTimePlayed(gprAudioManager.loadedStreams[0].handle) <= player->stats->overdriveActiveTime + OverdriveAnimDuration) {
		double TimeSinceOverdriveActivate = gprAudioManager.GetMusicTimePlayed(gprAudioManager.loadedStreams[0].handle) - player->stats->overdriveActiveTime;
		OverdriveAlpha = Remap(getEasingFunction(EaseOutQuint)(TimeSinceOverdriveActivate/OverdriveAnimDuration), 0, 1.0, 0, 255);
	} else OverdriveAlpha = 255;

	if (gprAudioManager.GetMusicTimePlayed(gprAudioManager.loadedStreams[0].handle) <= player->stats->overdriveActivateTime + OverdriveAnimDuration && gprAudioManager.GetMusicTimePlayed(gprAudioManager.loadedStreams[0].handle) > 0.0) {
		double TimeSinceOverdriveActivate = gprAudioManager.GetMusicTimePlayed(gprAudioManager.loadedStreams[0].handle) - player->stats->overdriveActivateTime;
		OverdriveAlpha = Remap(getEasingFunction(EaseOutQuint)(TimeSinceOverdriveActivate/OverdriveAnimDuration), 0, 1.0, 255, 0);
	} else if (!player->stats->Overdrive) OverdriveAlpha = 0;

	if (player->stats->Overdrive || gprAudioManager.GetMusicTimePlayed(gprAudioManager.loadedStreams[0].handle) <= player->stats->overdriveActivateTime + OverdriveAnimDuration) {DrawModel(gprAssets.odHighwayX, Vector3{0,0.001f,0},1,Color{255,255,255,OverdriveAlpha});}
	BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
	//DrawTriangle3D({lineDistance - 1.0f,0.003,0},
	//			   {lineDistance - 1.0f,0.003,(highwayLength *1.5f) + smasherPos},
	//			   {lineDistance,0.003,0},
	//			   Color{laneColor,laneColor,laneColor,laneAlpha});

	//DrawTriangle3D({lineDistance,0.003,(highwayLength *1.5f) + smasherPos},
	//			   {lineDistance,0.003,0},
	//			   {lineDistance - 1.0f,0.003,(highwayLength *1.5f) + smasherPos},
	//			   Color{laneColor,laneColor,laneColor,laneAlpha});

	//for (int i = 0; i < 4; i++) {
	//    float radius = player->ClassicMode ? 0.02 : ((i == (gprSettings.mirrorMode ? 2 : 1)) ? 0.05 : 0.02);
//
	//    DrawCylinderEx(Vector3{ lineDistance - (float)i, 0, smasherPos + 0.5f }, Vector3{ lineDistance - i, 0, (highwayLength *1.5f) + smasherPos }, radius, radius, 15, Color{ 128,128,128,128 });
	//}

	//DrawTriangle3D({0-lineDistance,0.003,0},
	//			   {0-lineDistance,0.003,(highwayLength *1.5f) + smasherPos},
	//			   {0-lineDistance + 1.0f,0.003,0},
	//			   Color{laneColor,laneColor,laneColor,laneAlpha});

	//DrawTriangle3D({0-lineDistance + 1.0f,0.003,(highwayLength *1.5f) + smasherPos},
	//			   {0-lineDistance + 1.0f,0.003,0},
	//			   {0-lineDistance,0.003,(highwayLength *1.5f) + smasherPos},
	//			   Color{laneColor,laneColor,laneColor,laneAlpha});

	if (!player->ClassicMode)
		DrawCylinderEx(Vector3{ lineDistance-1.0f, 0, smasherPos}, Vector3{ lineDistance-1.0f, 0, (highwayLength *1.5f) + smasherPos }, 0.025f, 0.025f, 15, Color{ 128,128,128,128 });

	EndBlendMode();

	EndMode3D();
	EndTextureMode();

	SetTextureWrap(highway_tex.texture,TEXTURE_WRAP_CLAMP);
	highway_tex.texture.width = (float)GetScreenWidth();
	highway_tex.texture.height = (float)GetScreenHeight();
	DrawTexturePro(highway_tex.texture, {0,0,(float)GetScreenWidth(), (float)-GetScreenHeight() },{ 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight() }, {renderPos,highwayLevel}, 0, WHITE );

	BeginBlendMode(BLEND_ALPHA);
	BeginTextureMode(highwayStatus_tex);
	ClearBackground({0,0,0,0});
	BeginMode3D(camera3pVector[cameraSel]);

	if (!song.beatLines.empty()) {
		DrawBeatlines(player, song, highwayLength, time);
	}

	if (!song.parts[player->Instrument]->charts[player->Difficulty].odPhrases.empty()) {
		DrawOverdrive(player, song.parts[player->Instrument]->charts[player->Difficulty], highwayLength, time);
	}
	if (!song.parts[player->Instrument]->charts[player->Difficulty].Solos.empty()) {
		DrawSolo(player, song.parts[player->Instrument]->charts[player->Difficulty], highwayLength, time);
	}

	float darkYPos = 0.015f;

	BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
	DrawTriangle3D({-diffDistance-0.5f,darkYPos,smasherPos},{-diffDistance-0.5f,darkYPos,(highwayLength *1.5f) + smasherPos},{diffDistance+0.5f,darkYPos,smasherPos},Color{0,0,0,64});
	DrawTriangle3D({diffDistance+0.5f,darkYPos,(highwayLength *1.5f) + smasherPos},{diffDistance+0.5f,darkYPos,smasherPos},{-diffDistance-0.5f,darkYPos,(highwayLength *1.5f) + smasherPos},Color{0,0,0,64});
	EndBlendMode();
	EndMode3D();
	EndTextureMode();
	SetTextureWrap(highwayStatus_tex.texture,TEXTURE_WRAP_CLAMP);
	highwayStatus_tex.texture.width = (float)GetScreenWidth();
	highwayStatus_tex.texture.height = (float)GetScreenHeight();
	DrawTexturePro(highwayStatus_tex.texture, {0,0,(float)GetScreenWidth(), (float)-GetScreenHeight() },{ 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight() }, {renderPos,highwayLevel}, 0, WHITE );

	BeginTextureMode(smasher_tex);
	ClearBackground({0,0,0,0});
	BeginMode3D(camera3pVector[cameraSel]);
	BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
	DrawModel(gprAssets.smasherBoard, Vector3{ 0, 0.004f, 0 }, 1.0f, WHITE);
	BeginBlendMode(BLEND_ALPHA);
	for (int i = 0; i < 4; i++) {
		Color NoteColor = RED;
		if (i == 1) NoteColor = YELLOW;
		else if (i == 2) NoteColor = BLUE;
		else if (i == 3) NoteColor = GREEN;
		gprAssets.smasherPressed.materials[0].maps[MATERIAL_MAP_ALBEDO].color = NoteColor;
		gprAssets.smasherReg.materials[0].maps[MATERIAL_MAP_ALBEDO].color = NoteColor;

		if (stats->HeldFrets[i] || stats->HeldFretsAlt[i]) {
			DrawModel(gprAssets.smasherPressed, Vector3{ (diffDistance - (float)(i*1.25))-0.125f, 0.01f, smasherPos }, 1.25f, WHITE);
		}
		else {
			DrawModel(gprAssets.smasherReg, Vector3{ (diffDistance - (float)(i*1.25))-0.125f, 0.01f, smasherPos }, 1.25f, WHITE);

		}
	}
	//DrawModel(gprAssets.lanes, Vector3 {0,0.1f,0}, 1.0f, WHITE);

	EndBlendMode();
	EndMode3D();
	EndTextureMode();
	SetTextureWrap(smasher_tex.texture,TEXTURE_WRAP_CLAMP);
	smasher_tex.texture.width = (float)GetScreenWidth();
	smasher_tex.texture.height = (float)GetScreenHeight();
	DrawTexturePro(smasher_tex.texture, {0,0,(float)GetScreenWidth(), (float)-GetScreenHeight() },{ 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight() }, {renderPos,highwayLevel}, 0, WHITE );
}