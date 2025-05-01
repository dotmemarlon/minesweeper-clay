#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_timer.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

#define CLAY_IMPLEMENTATION
#include "clay.h"
#include "clay_renderer_SDL3.c"

static const Uint32 FONT_ID = 0;

static const Clay_Color COLOR_ORANGE = (Clay_Color){225, 138, 50, 255};
static const Clay_Color COLOR_BLUE = (Clay_Color){111, 173, 162, 255};
static const Clay_Color COLOR_LIGHT = (Clay_Color){224, 215, 210, 255};

#define LENGTH_OF(x) (sizeof(x) / sizeof(*x))


typedef struct TextureState {
	SDL_Texture* unopened_square;
	SDL_Texture* opened_square;
	SDL_Texture* flagged_square;
	SDL_Texture* bomb_square;
	SDL_Texture* last_clicked_bomb_square;
	SDL_Texture* not_bomb_square;
	SDL_Texture* square_numbers[8];
	SDL_Texture* ui_numbers[12];
	SDL_Texture* emoji_faces[5];
} TextureState;

typedef enum SquareState {
	SQUARESTATE_OPENED = 1 << 0,
	SQUARESTATE_FLAGGED = 1 << 1,
	SQUARESTATE_ISBOMB = 1 << 2,
	SQUARESTATE_LASTCLICKED = 1 << 3
} SquareState;

typedef struct BoardState {
	int ** board;
	uint8_t col_num;
	uint8_t row_num;
} BoardState;

typedef struct MinuteSecondTime {
	uint8_t minute;
	uint8_t second;
} MinuteSecondTime;

typedef struct GameState{
	uint64_t timer_start;
	uint64_t timer_end;
	int bomb_num;
	SDL_MouseButtonFlags mouse_button_flag;
	bool is_game_over;
	bool is_emoji_clicked;
} GameState;

MinuteSecondTime SecondToMinuteSecondTime(uint64_t sec) {
	MinuteSecondTime ret;
	ret.minute = sec / 60;
	ret.second = sec - (int)(sec / 60) * 60;
	return ret;
}

bool IsContainFlag(int square, int state) {
	if ((square | state) == square) return true;
	return false;
}

int IsValidPosition(BoardState board_state, int idx, int jdx) {
	if (idx >= 0 && idx < board_state.row_num && jdx >= 0 && jdx < board_state.col_num) return 1;
	return 0;
}

int GetAdjacentBombs(BoardState board_state, int idx, int jdx) {
	if (!IsValidPosition(board_state, idx, jdx)) return -1;	
	int num = 0;

	int bi = idx - 1;
	int bj = jdx - 1;

	int ei = idx + 1;
	int ej = jdx + 1;

	for (int i = bi; i <= ei; ++i) {
		for (int j = bj; j <= ej; ++j) {
			if (!IsValidPosition(board_state, i, j)) continue;
			if (IsContainFlag(board_state.board[i][j], SQUARESTATE_ISBOMB)) num++;
		}
	}

	return num;
}

void SafelyExposeAdjacentSquare(BoardState board_state, int idx, int jdx) {
	if (!IsValidPosition(board_state, idx, jdx)) return;

	if (IsContainFlag(board_state.board[idx][jdx], SQUARESTATE_OPENED)) return;
	board_state.board[idx][jdx] |= SQUARESTATE_OPENED;

	int adjacent_bombs = GetAdjacentBombs(board_state, idx, jdx);
	if (adjacent_bombs != 0) return;
	
	int bi = idx - 1;
	int bj = jdx - 1;

	int ei = idx + 1;
	int ej = jdx + 1;

	for (int i = bi; i <= ei; ++i) {
		for (int j = bj; j <= ej; ++j) {
			SafelyExposeAdjacentSquare(board_state, i, j);
		}
	}
}



typedef struct app_state {
  SDL_Window *window;
  SDL_Renderer *renderer;
	TextureState textures;
	BoardState board;
	GameState game;
} AppState;

static void HandleClayErrors(Clay_ErrorData errorData) {
	printf("%s", errorData.errorText.chars);
}

static inline int max(int a, int b) {
	if (a > b) return a;
	return b;
}

static inline int min(int a, int b) {
	if (a < b) return a;
	return b;
}

void ArrShuffle(int* arr, int n)
{
	if (n > 1) 
	{
		size_t i;
		for (i = 0; i < n - 1; i++) 
		{
			size_t j = i + (rand() / (RAND_MAX / (n - i) + 1));
			int t = arr[j];
			arr[j] = arr[i];
			arr[i] = t;
		}
	}
} 

void GeneratePuzzle(void* appstate) {
	AppState *state = appstate;
#if 1
	char cmd[102] = "";
	sprintf(cmd, "python3 gen.py %d %d > board_map.txt", state->board.row_num, state->board.col_num);
	system(cmd);
	FILE* f = fopen("board_map.txt", "r");
	state->game.bomb_num = 0;
	state->game.is_game_over = 0;
	state->game.mouse_button_flag = 0;
	state->game.is_emoji_clicked = 0;
	state->game.timer_start = SDL_GetTicks();
	state->game.timer_end = SDL_GetTicks();
	for (int i = 0;i < state->board.row_num; ++i) {
		for (int j = 0;j < state->board.col_num; ++j) {
			fscanf(f, "%d", &state->board.board[i][j]);
			if (state->board.board[i][j]) {
				state->board.board[i][j] = SQUARESTATE_ISBOMB;
				state->game.bomb_num++;
			}
		}
	}
	fclose(f);
#else
	state->game.bomb_num = rand() % (state->board.col_num * state->board.row_num - 5) + 5;
	int* arr1d = SDL_malloc(sizeof(int) * state->board.col_num * state->board.row_num);
	for (int i = 0;i < state->board.col_num * state->board.row_num; ++i) {
		arr1d[i] = i;
	}
	ArrShuffle(arr1d, state->board.col_num * state->board.row_num);
	for (int i = 0;i < state->board.col_num * state->board.row_num; ++i) {
		state->board.board[i / state->board.col_num][i % state->board.col_num] = 0;
	}
	for (int i = 0;i < state->game.bomb_num; ++i) {
		state->board.board[arr1d[i] / state->board.col_num][arr1d[i] % state->board.col_num] = SQUARESTATE_ISBOMB;
	}
	SDL_free(arr1d);
#endif
}

void ClayUIBar( void* appstate, MyClay_CustomElementConfig* small_border_custom_config) {
	AppState* state = appstate;
	TextureState texture_state = state->textures;
	CLAY({
		.id=CLAY_ID("UIBar"),
		.layout={
			.layoutDirection=CLAY_LEFT_TO_RIGHT,
			.sizing={CLAY_SIZING_GROW(0), CLAY_SIZING_PERCENT(0.125)}, 
			.childGap=32, 
			.padding={16,16,16,16},
			.childAlignment={.y=CLAY_ALIGN_Y_TOP}	
		},
		.custom = {small_border_custom_config}
	}) {
		Clay_BoundingBox ui_bar_box = Clay_GetElementData(CLAY_ID("UIBar")).boundingBox;
		ui_bar_box.height -= 16 * 2;
		ui_bar_box.y += 16;
		ui_bar_box.width -= 16*2;
		ui_bar_box.x += 16;
		float ui_char_width = ui_bar_box.height / texture_state.ui_numbers[0]->h * texture_state.ui_numbers[0]->w;

		CLAY({
			.id=CLAY_ID("BombCounterBox"),
			.layout={
				.layoutDirection=CLAY_LEFT_TO_RIGHT,
				.sizing={CLAY_SIZING_FIXED(ui_char_width*3), CLAY_SIZING_FIXED(ui_bar_box.height)}
			},
			.backgroundColor=(Clay_Color){0x0,0x0,0x0,0xff}	
		}) {
			SDL_Texture* nums[3];	
			int bomb_num = state->game.bomb_num;
			nums[0] = texture_state.ui_numbers[0];
			if (bomb_num < 0) nums[0] = texture_state.ui_numbers[10];
			nums[1] = texture_state.ui_numbers[SDL_abs(bomb_num) / 10];
			nums[2] = texture_state.ui_numbers[SDL_abs(bomb_num) % 10];

			for (int i = 0;i < 3; ++i) {
				CLAY({
					.layout={
						.sizing={CLAY_SIZING_FIXED(ui_char_width), CLAY_SIZING_FIXED( ui_bar_box.height)}
					},
					.image={nums[i], (Clay_Dimensions){nums[i]->w, nums[i]->h}}
				}) {}
			}
		}
		CLAY({.layout={.sizing={CLAY_SIZING_GROW(0)}}}) {}
		SDL_Texture* emoji_face = state->textures.emoji_faces[0];
		if (state->game.is_game_over) {
			emoji_face = state->textures.emoji_faces[4];
		}
		else if (state->game.is_emoji_clicked) {
			emoji_face = state->textures.emoji_faces[1];
		}
		CLAY({
			.id=CLAY_ID("EmojiBox"),
			.layout={
				.sizing={CLAY_SIZING_FIXED(ui_bar_box.height), CLAY_SIZING_FIXED(ui_bar_box.height)}
			},
			.backgroundColor=(Clay_Color){0x1f,0xef,0xef,0xff},
			.image={emoji_face, (Clay_Dimensions){emoji_face->w, emoji_face->h}}
		}) {
			if (Clay_Hovered()) {
				if (state->game.mouse_button_flag == SDL_BUTTON_LMASK) {
					state->game.is_emoji_clicked = 1;
				} else if (state->game.is_emoji_clicked && state->game.mouse_button_flag == 0) {
					state->game.mouse_button_flag = 0;
					state->game.is_emoji_clicked = 0;
					GeneratePuzzle(state);
				}
			} else {
				state->game.is_emoji_clicked = 0;
			}
		}
		CLAY({.layout={.sizing={CLAY_SIZING_GROW(0)}}}) {}
		CLAY({
			.id=CLAY_ID("TimerBox"),
			.layout={
				.sizing={CLAY_SIZING_FIXED(ui_char_width * 6), CLAY_SIZING_FIXED(ui_bar_box.height)}
			},
			.backgroundColor=(Clay_Color){0x1f,0xef,0xef,0xff}	

		}) {
			SDL_Texture* texts[6];
			if (!state->game.is_game_over) state->game.timer_end = SDL_GetTicks();
			MinuteSecondTime ms_time = SecondToMinuteSecondTime((state->game.timer_end-state->game.timer_start)/1000);
			texts[0] = texture_state.ui_numbers[(ms_time.minute/100) % 10];
			texts[1] = texture_state.ui_numbers[(ms_time.minute/10) % 10];
			texts[2] = texture_state.ui_numbers[(ms_time.minute) % 10];
			texts[3] = texture_state.ui_numbers[11];
			texts[4] = texture_state.ui_numbers[(ms_time.second/10) % 10];
			texts[5] = texture_state.ui_numbers[(ms_time.second) % 10];
			for (int i = 0;i < 6; ++i) {
				CLAY({
					.layout={
						.sizing={CLAY_SIZING_FIXED(ui_char_width), CLAY_SIZING_FIXED( ui_bar_box.height)}
					},
					.image={texts[i], (Clay_Dimensions){texts[i]->w, texts[i]->h}}
				}) {}
			}
		}
	}
}

void LoadTextures_IMG(SDL_Renderer * renderer, SDL_Texture** textures, int size,const char* path) {
	char file[100];
	strcpy(file, path);
	for (int i = 0;i < size; ++i) {
		sprintf(file, "%s/%d.png", path, i);
		textures[i] = IMG_LoadTexture(renderer, file);
		SDL_SetTextureScaleMode(textures[i], SDL_SCALEMODE_NEAREST);
	}
}

static inline Clay_Dimensions SDL_MeasureText(Clay_StringSlice text,
                                              Clay_TextElementConfig *config,
                                              void *userData) {
  TTF_Font *font = g_fonts[config->fontId];
  int width, height;

  if (!TTF_GetStringSize(font, text.chars, text.length, &width, &height)) {
    SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to measure text: %s",
                 SDL_GetError());
  }

  return (Clay_Dimensions){(float)width, (float)height};
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv) {
  (void)argc;
  (void)argv;

  if (!TTF_Init()) return SDL_APP_FAILURE;
  if (!SDL_Init(SDL_INIT_VIDEO)) return SDL_APP_FAILURE;

  AppState *state = SDL_calloc(1, sizeof(*state));
  if (!state) return SDL_APP_FAILURE;

  if (!SDL_CreateWindowAndRenderer("Test clay", 800, 600, 0,
                                   &state->window, &state->renderer)) {
    SDL_LogError(SDL_LOG_CATEGORY_ERROR,
                 "Failed to create window and renderer: %s", SDL_GetError());
    return SDL_APP_FAILURE;

  }
	SDL_SetWindowResizable(state->window,1);
	SDL_SetRenderVSync(state->renderer, SDL_RENDERER_VSYNC_ADAPTIVE);

  TTF_Font *font = TTF_OpenFont("ComicMono.ttf", 24);
  if (!font) {
    SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to load font %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }
  g_fonts[0] = font;

  uint64_t total_memory_size = Clay_MinMemorySize();
  Clay_Arena clay_memory = (Clay_Arena){.memory = SDL_malloc(total_memory_size),
                                        .capacity = total_memory_size};

  int width, height;
  SDL_GetWindowSize(state->window, &width, &height);
  Clay_Initialize(
      clay_memory, (Clay_Dimensions){width, height},
      (Clay_ErrorHandler){.errorHandlerFunction = HandleClayErrors});

  Clay_SetMeasureTextFunction(SDL_MeasureText, 0);

	state->textures.flagged_square = IMG_LoadTexture(state->renderer, "assets/flagged_square.png");
	state->textures.opened_square = IMG_LoadTexture(state->renderer, "assets/opened_square.png");
	state->textures.unopened_square = IMG_LoadTexture(state->renderer, "assets/unopened_square.png");
	state->textures.bomb_square = IMG_LoadTexture(state->renderer, "assets/bomb_square.png");
	state->textures.not_bomb_square = IMG_LoadTexture(state->renderer, "assets/not_bomb_square.png");
	state->textures.last_clicked_bomb_square = IMG_LoadTexture(state->renderer, "assets/last_clicked_bomb_square.png");
	LoadTextures_IMG(state->renderer, state->textures.square_numbers, 8, "assets/square_numbers/");
	LoadTextures_IMG(state->renderer, state->textures.ui_numbers, 12, "assets/ui_numbers/");
	LoadTextures_IMG(state->renderer, state->textures.emoji_faces, 5, "assets/emoji_faces/");


	SDL_SetTextureScaleMode(state->textures.unopened_square, SDL_SCALEMODE_NEAREST);
	SDL_SetTextureScaleMode(state->textures.opened_square, SDL_SCALEMODE_NEAREST);
	SDL_SetTextureScaleMode(state->textures.flagged_square, SDL_SCALEMODE_NEAREST);
	SDL_SetTextureScaleMode(state->textures.bomb_square, SDL_SCALEMODE_NEAREST);
	SDL_SetTextureScaleMode(state->textures.last_clicked_bomb_square, SDL_SCALEMODE_NEAREST);
	SDL_SetTextureScaleMode(state->textures.not_bomb_square, SDL_SCALEMODE_NEAREST);

	state->game.bomb_num = 0;
	state->game.is_game_over = 0;
	state->game.timer_start = 0;
	state->game.timer_end = 0;
	state->game.mouse_button_flag = 0;

	state->board.col_num = 16;
	state->board.row_num = 8;
	state->board.board = SDL_malloc(sizeof(*state->board.board) * state->board.row_num);
	for (int i = 0;i < state->board.row_num; ++i) {
		state->board.board[i] = SDL_malloc(sizeof(*state->board.board[0]) * state->board.col_num);
	}

	GeneratePuzzle(state);

  *appstate = state;

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
	SDL_AppResult ret = SDL_APP_CONTINUE;	
	AppState* state = appstate;

	switch (event->type) {
		Clay_Vector2 mouse_pos;
		case SDL_EVENT_QUIT:
			ret = SDL_APP_SUCCESS;
			break;
		case SDL_EVENT_WINDOW_RESIZED:
			Clay_SetLayoutDimensions((Clay_Dimensions){event->window.data1, event->window.data2});
			break;
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
			state->game.mouse_button_flag = SDL_GetMouseState(0, 0);
			break;
		case SDL_EVENT_MOUSE_BUTTON_UP:
			state->game.mouse_button_flag = SDL_GetMouseState(0, 0);
			break;
		case SDL_EVENT_MOUSE_MOTION:
			SDL_GetMouseState(&mouse_pos.x, &mouse_pos.y);
			Clay_SetPointerState(mouse_pos, state->game.mouse_button_flag != 0);
			break;
	}

	return ret;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
	SDL_AppResult ret = SDL_APP_CONTINUE;
	AppState* state = appstate;

	struct {
		int width, height;
	} window_size;
	SDL_GetWindowSize(state->window, &window_size.width, &window_size.height);
	float medium_border_size = (float)min(window_size.width, window_size.height) * 0.01;
	float small_border_size = (float)min(window_size.width, window_size.height) * 0.007;

	Clay_BeginLayout();

	MyClay_Border2ColorElementConfig medium_border_custom_config_data = 
		{{0x80,0x80,0x80,0xff}, {0xff,0xff,0xff,0xff}, medium_border_size};
	MyClay_CustomElementConfig medium_border_custom_config = 
		{&medium_border_custom_config_data, MYCLAY_RENDER_COMMAND_TYPE_BORDER_2_COLOR};


	MyClay_Border2ColorElementConfig square_custom_data = 
		{ {0xff,0xff,0xff,0xff},{0x80,0x80,0x80,0xff}, small_border_size};
	MyClay_CustomElementConfig square_custom_config = 
		{&square_custom_data, MYCLAY_RENDER_COMMAND_TYPE_BORDER_2_COLOR};

	MyClay_Border2ColorElementConfig small_border_custom_config_data = 
		{{0x80,0x80,0x80,0xff}, {0xff,0xff,0xff,0xff}, small_border_size};
	MyClay_CustomElementConfig small_border_custom_config = 
		{&small_border_custom_config_data, MYCLAY_RENDER_COMMAND_TYPE_BORDER_2_COLOR};

	CLAY( {
		.id = CLAY_ID("OuterContainer"), 
		.layout = {
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
			.sizing = {CLAY_SIZING_PERCENT(1), CLAY_SIZING_PERCENT(1)},
			.childGap = 16,
			.padding = {16 + medium_border_size,16,16 + medium_border_size,16}
		}, 
		.backgroundColor = {0xc0,0xc0,0xc0,0xff},
		.border = {{0xff,0xff,0xff,0xff}, {medium_border_size,0,medium_border_size,0}}
	}) 
	{
		ClayUIBar(state,&small_border_custom_config);
		CLAY({
			.id = CLAY_ID("BoardContainer"),
			.layout = {
				.layoutDirection = CLAY_LEFT_TO_RIGHT,
				.sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_PERCENT(0.865) },
				.childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}
			},
			//.backgroundColor = {0x8f,0x8f,0x8f,0xff},
			.custom = {&medium_border_custom_config},
			
		}) 
		{
			Clay_BoundingBox container_box = Clay_GetElementData(CLAY_ID("BoardContainer")).boundingBox;
			container_box.x += medium_border_size;
			container_box.y += medium_border_size;
			container_box.width -= medium_border_size * 2;
			container_box.height -= medium_border_size * 2;
			int column = state->board.col_num;
			int row = state->board.row_num;
			float square_real_size = fminf(container_box.width / column, container_box.height / row);


			CLAY({
				.id = CLAY_ID("BoardInContainer"),
				.layout = {
					.sizing = {CLAY_SIZING_FIXED(square_real_size * column), CLAY_SIZING_FIXED(square_real_size*row)}
				},
			}) {
				BoardState board = state->board;
				TextureState textures = state->textures;
				for (int i = 0;i < row; ++i) {
					for (int j = 0;j < column; ++j) {
						SDL_Texture* square_txt = textures.unopened_square;
						if (IsContainFlag(board.board[i][j], SQUARESTATE_OPENED)) {
							if (IsContainFlag(board.board[i][j], SQUARESTATE_ISBOMB)) {
								square_txt = textures.bomb_square;
								if (IsContainFlag(board.board[i][j], SQUARESTATE_LASTCLICKED))
									square_txt = textures.last_clicked_bomb_square;
							} 
							else {
								square_txt = textures.opened_square;
								int adjacent_bombs = GetAdjacentBombs(board, i,  j);
								if (adjacent_bombs != 0) {
									square_txt = textures.square_numbers[adjacent_bombs-1];
								}
							}
						} else if (IsContainFlag(board.board[i][j], SQUARESTATE_FLAGGED)) {
							square_txt = textures.flagged_square;
							if (state->game.is_game_over) {
								square_txt = textures.not_bomb_square;
							}
						}
						CLAY({
							.layout = {
								.sizing = { CLAY_SIZING_FIXED(square_real_size), CLAY_SIZING_FIXED(square_real_size)}
							},
							.backgroundColor = {0xc0,0xc0,0xc0,0xff},
							.floating = {
								.offset = {square_real_size * j, square_real_size * i},
								.attachTo = CLAY_ATTACH_TO_PARENT,
								.attachPoints = {CLAY_ATTACH_POINT_LEFT_TOP, CLAY_ATTACH_POINT_LEFT_TOP}
							},
							//.custom = {&square_custom_config},
							.image = {square_txt, (Clay_Dimensions){square_txt->w,square_txt->h}}
							
						}) {
							if (Clay_Hovered() && !state->game.is_game_over) {
								if (state->game.mouse_button_flag == SDL_BUTTON_LMASK) {
									state->game.mouse_button_flag = 0;
									if (!IsContainFlag(board.board[i][j], SQUARESTATE_OPENED) &&
											!IsContainFlag(board.board[i][j], SQUARESTATE_FLAGGED)) {
										if (!IsContainFlag(board.board[i][j], SQUARESTATE_ISBOMB)) {
											SafelyExposeAdjacentSquare(board, i, j);
										} else {
											state->game.is_game_over = 1;
											board.board[i][j] |= SQUARESTATE_LASTCLICKED;
											for (int i = 0;i < state->board.row_num; ++i)
												for (int j= 0;j < state->board.col_num; ++j) {
													if (IsContainFlag(state->board.board[i][j], SQUARESTATE_ISBOMB))
														board.board[i][j] |= SQUARESTATE_OPENED;
												}
										}
									}
								}
								else if (state->game.mouse_button_flag == SDL_BUTTON_RMASK) {
									state->game.mouse_button_flag = 0;
									if (!IsContainFlag(board.board[i][j], SQUARESTATE_OPENED)) {
										if (!IsContainFlag(board.board[i][j], SQUARESTATE_FLAGGED)) {
											board.board[i][j] |= SQUARESTATE_FLAGGED;
											state->game.bomb_num--;
										} else {
											board.board[i][j] ^= SQUARESTATE_FLAGGED;
											state->game.bomb_num++;
										}
									}
								}
							}
						}
					}
				}
			}
		}

		CLAY({
			.id = CLAY_ID("EndPadding"),
			.layout = { .sizing = { .width=CLAY_SIZING_GROW(0), .height=CLAY_SIZING_PERCENT(0.01) } }
		}) {
		}
	}

	Clay_RenderCommandArray render_command_array = Clay_EndLayout();

	SDL_SetRenderDrawColor(state->renderer, 0x0, 0x0, 0x0, 0xff);
	SDL_RenderClear(state->renderer);
	SDL_RenderClayCommands(state->renderer, &render_command_array);
	SDL_RenderPresent(state->renderer);

	return ret;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
	if (result != SDL_APP_SUCCESS) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "App failed to run");
	}
	AppState* state = appstate;
	if (state) {
		SDL_DestroyRenderer(state->renderer);
		SDL_DestroyWindow(state->window);
		SDL_DestroyTexture(state->textures.flagged_square);
		SDL_DestroyTexture(state->textures.unopened_square);
		SDL_DestroyTexture(state->textures.opened_square);

		for (int i = 0;i < LENGTH_OF(state->textures.square_numbers); ++i) {
			SDL_DestroyTexture(state->textures.square_numbers[i]);
		}

		for (int i = 0;i < LENGTH_OF(state->textures.ui_numbers); ++i) {
			SDL_DestroyTexture(state->textures.ui_numbers[i]);
		}

		for (int i = 0;i < LENGTH_OF(state->textures.emoji_faces); ++i) {
			SDL_DestroyTexture(state->textures.emoji_faces[i]);
		}

		for (int i = 0;i < state->board.row_num; ++i) {
			SDL_free(state->board.board[i]);
		}
		SDL_free(state->board.board);

		SDL_free(state);
	}
	TTF_CloseFont(g_fonts[0]);
	TTF_Quit();
	SDL_Quit();
}





