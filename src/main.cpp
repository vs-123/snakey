#include <raylib.h>
#include <vector>
#include <random>
#include <chrono>
#include <string>
#include <algorithm>

std::string key_code_to_string(int key) {
  switch (key) {
    case KEY_ESCAPE:  return "ESC";
    case KEY_UP:      return "UP";
    case KEY_DOWN:    return "DOWN";
    case KEY_LEFT:    return "LEFT";
    case KEY_RIGHT:   return "RIGHT";
    case KEY_W:       return "W";
    case KEY_A:       return "A";
    case KEY_S:       return "S";
    case KEY_D:       return "D";
    default:          return "Key " + std::to_string(key);
  }
}

// Returns a button colour based on mouse position
// This simulates hover effect
Color get_button_color(const Rectangle &rect) {
  return CheckCollisionPointRec(GetMousePosition(), rect) ? GRAY : LIGHTGRAY;
}

// Constants
constexpr int BLOCK_SIZE       = 20;
constexpr int GRID_WIDTH       = 40;   // 800 / 20
constexpr int GRID_HEIGHT      = 30;   // 600 / 20
constexpr int SCREEN_WIDTH     = GRID_WIDTH * BLOCK_SIZE;
constexpr int SCREEN_HEIGHT    = GRID_HEIGHT * BLOCK_SIZE;

constexpr int BUTTON_WIDTH     = 200;
constexpr int BUTTON_HEIGHT    = 50;

enum class GameState {
  StartMenu,
  Settings,
  Keybinds,
  Countdown,
  Playing,
  Pause,
  ConfirmRestart,
  ConfirmMainMenu,
  GameOver
};

struct Point {
  int x;
  int y;
};

enum class Direction {
  Up,
  Down,
  Left,
  Right
};

// Structure for key bindings
struct KeyBindings {
  std::vector<int> pause;
  std::vector<int> resume;
  std::vector<int> up;
  std::vector<int> down;
  std::vector<int> left;
  std::vector<int> right;

  KeyBindings() {
    pause  = { KEY_ESCAPE };
    resume = { KEY_ESCAPE };
    up     = { KEY_UP, KEY_W };
    down   = { KEY_DOWN, KEY_S };
    left   = { KEY_LEFT, KEY_A };
    right  = { KEY_RIGHT, KEY_D };
  }
};

// The Snake
class Snake {
public:
  Snake()
    : current_direction(Direction::Right),
      grow_snake(false)
  {
    int init_x = GRID_WIDTH / 2;
    int init_y = GRID_HEIGHT / 2;
    segments.push_back({ init_x, init_y });
  }

  // Constructor with an initial length
  Snake(int init_length)
    : current_direction(Direction::Right),
      grow_snake(false)
  {
    int init_x = GRID_WIDTH / 2;
    int init_y = GRID_HEIGHT / 2;
    for (int i = 0; i < init_length; ++i) {
      segments.push_back({ init_x - i, init_y });
    }
  }

  Point get_head() const { return segments.front(); }

  void update() {
    Point new_head = segments.front();
    switch (current_direction) {
      case Direction::Up:    new_head.y--; break;
      case Direction::Down:  new_head.y++; break;
      case Direction::Left:  new_head.x--; break;
      case Direction::Right: new_head.x++; break;
    }
    segments.insert(segments.begin(), new_head);
    if (!grow_snake) {
      segments.pop_back();
    } else {
      grow_snake = false;
    }
  }

  void set_head(const Point &new_head) { segments.front() = new_head; }

  void draw() const {
    for (const auto &segment : segments) {
      DrawRectangle(segment.x * BLOCK_SIZE, segment.y * BLOCK_SIZE,
                      BLOCK_SIZE, BLOCK_SIZE, GREEN);
    }
  }

  void set_direction(Direction new_direction) {
    if ((current_direction == Direction::Up && new_direction == Direction::Down) ||
        (current_direction == Direction::Down && new_direction == Direction::Up) ||
        (current_direction == Direction::Left && new_direction == Direction::Right) ||
        (current_direction == Direction::Right && new_direction == Direction::Left))
    {
      return;
    }
    current_direction = new_direction;
  }

  void grow() { grow_snake = true; }

  bool has_self_collision() const {
    const auto head = segments.front();
    for (size_t i = 1; i < segments.size(); ++i) {
      if (segments[i].x == head.x && segments[i].y == head.y) { return true; }
    }
    return false;
  }

  int get_length() const { return static_cast<int>(segments.size()); }

private:
  std::vector<Point> segments;
  Direction current_direction;
  bool grow_snake;
};

// The Food
class Food {
public:
  Food() { respawn(); }
  const Point &get_position() const { return position; }
  void respawn() {
    static std::mt19937 random_engine{ std::random_device{}() };
    std::uniform_int_distribution<int> dist_x(0, GRID_WIDTH - 1);
    std::uniform_int_distribution<int> dist_y(0, GRID_HEIGHT - 1);
    position.x = dist_x(random_engine);
    position.y = dist_y(random_engine);
  }

  void draw() const {
    DrawRectangle(position.x * BLOCK_SIZE, position.y * BLOCK_SIZE,
                  BLOCK_SIZE, BLOCK_SIZE, RED);
  }

private:
  Point position;
};

// The Main Game
class Game {
private:
  GameState app_state;
  GameState previous_state;  // Used for returning from Settings
  int initial_snake_length;
  int tick_rate_ms;
  bool wrapping_enabled;
  int best_length;
  const int countdown_duration_ms;
  std::chrono::steady_clock::time_point countdown_start_time;
  Snake snake{ initial_snake_length };
  Food food;
  std::chrono::steady_clock::time_point last_move_time;
  KeyBindings key_bindings;
  int current_edit_action;  // Used for keybind editing
  RenderTexture2D pause_texture;

public:
  Game()
    : app_state(GameState::StartMenu),
      previous_state(GameState::StartMenu),
      initial_snake_length(3),
      tick_rate_ms(100),
      wrapping_enabled(true),
      best_length(0),
      countdown_duration_ms(3000),
      countdown_start_time(std::chrono::steady_clock::now()),
      current_edit_action(-1)
  {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "SNAKEY");
    SetTargetFPS(60);
    SetExitKey(0);  // Disable ESC from closing the window
  }

  ~Game() {
    UnloadRenderTexture(pause_texture);
    CloseWindow();
  }

  void run() {
    while (!WindowShouldClose()) {
      update();
      draw();
    }
  }

private:
  bool is_action_down(const std::vector<int>& keys) {
    for (int key : keys) { if (IsKeyDown(key)) return true; }
    return false;
  }

  bool is_action_pressed(const std::vector<int>& keys) {
    for (int key : keys) { if (IsKeyPressed(key)) return true; }
    return false;
  }

  bool is_mouse_in_rect(const Rectangle &rect) {
    return CheckCollisionPointRec(GetMousePosition(), rect);
  }

  // Update functions for each state.
  void update() {
    switch (app_state) {
      case GameState::StartMenu:      update_start_menu(); break;
      case GameState::Settings:       update_settings(); break;
      case GameState::Keybinds:       update_keybinds(); break;
      case GameState::Countdown:      update_countdown(); break;
      case GameState::Playing:        update_playing(); break;
      case GameState::Pause:          update_pause(); break;
      case GameState::ConfirmRestart: update_confirm_restart(); break;
      case GameState::ConfirmMainMenu:update_confirm_main_menu(); break;
      case GameState::GameOver:       update_game_over(); break;
    }
  }

  void update_start_menu() {
    const int button_count = 3;
    const int spacing = 20;
    int total_height = button_count * BUTTON_HEIGHT + (button_count - 1) * spacing;
    int start_y = (SCREEN_HEIGHT - total_height) / 2;
    int start_x = SCREEN_WIDTH / 2 - BUTTON_WIDTH / 2;
    Rectangle play_button = { (float)start_x, (float)start_y, (float)BUTTON_WIDTH, (float)BUTTON_HEIGHT };
    Rectangle settings_button = { (float)start_x, (float)(start_y + BUTTON_HEIGHT + spacing), (float)BUTTON_WIDTH, (float)BUTTON_HEIGHT };
    Rectangle quit_button = { (float)start_x, (float)(start_y + 2 * (BUTTON_HEIGHT + spacing)), (float)BUTTON_WIDTH, (float)BUTTON_HEIGHT };

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
      if (is_mouse_in_rect(play_button)) {
        countdown_start_time = std::chrono::steady_clock::now();
        app_state = GameState::Countdown;
      } else if (is_mouse_in_rect(settings_button)) {
        previous_state = GameState::StartMenu;
        app_state = GameState::Settings;
      } else if (is_mouse_in_rect(quit_button)) {
        CloseWindow();
      }
    }
  }

  void update_settings() {
    Rectangle snake_length_slider = { 100, 150, 200, 10 };
    Rectangle tick_rate_slider = { 100, 250, 200, 10 };
    Rectangle wrapping_checkbox = { 100, 350, 20, 20 };
    Rectangle keybinds_button = { 100, 410, 200, 40 };
    Vector2 mouse_pos = GetMousePosition();
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
      if (CheckCollisionPointRec(mouse_pos, snake_length_slider)) {
        float pos = (mouse_pos.x - snake_length_slider.x) / snake_length_slider.width;
        int new_value = 1 + static_cast<int>(pos * 9.0f);
        initial_snake_length = std::clamp(new_value, 1, 10);
      }
      if (CheckCollisionPointRec(mouse_pos, tick_rate_slider)) {
        float pos = (mouse_pos.x - tick_rate_slider.x) / tick_rate_slider.width;
        int new_rate = 50 + static_cast<int>(pos * 450.0f);
        tick_rate_ms = std::clamp(new_rate, 50, 500);
      }
    }
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
      if (is_mouse_in_rect(wrapping_checkbox)) {
        wrapping_enabled = !wrapping_enabled;
      }
      if (is_mouse_in_rect(keybinds_button)) {
        app_state = GameState::Keybinds;
      }
      Rectangle back_button = { SCREEN_WIDTH - 120, SCREEN_HEIGHT - 60, 100, 40 };
      if (is_mouse_in_rect(back_button)) {
        app_state = previous_state;
      }
    }
  }

  void update_keybinds() {
    int start_y = 100, spacing = 50, start_x = 100, entry_width = 400;
    Rectangle back_button = { SCREEN_WIDTH - 120, SCREEN_HEIGHT - 60, 100, 40 };
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
      Vector2 mouse_pos = GetMousePosition();
      for (int i = 0; i < 6; ++i) {
        Rectangle entry_rect = { float(start_x), float(start_y + i * spacing), float(entry_width), 40 };
        if (CheckCollisionPointRec(mouse_pos, entry_rect)) { current_edit_action = i; break; }
      }
      if (is_mouse_in_rect(back_button)) { app_state = GameState::Settings; }
    }
    if (current_edit_action != -1) {
      int new_key = GetKeyPressed();
      if (new_key != 0) {
        switch (current_edit_action) {
          case 0: key_bindings.pause  = { new_key }; break;
          case 1: key_bindings.resume = { new_key }; break;
          case 2: key_bindings.up     = { new_key }; break;
          case 3: key_bindings.down   = { new_key }; break;
          case 4: key_bindings.left   = { new_key }; break;
          case 5: key_bindings.right  = { new_key }; break;
        }
        current_edit_action = -1;
      }
    }
  }

  void update_countdown() {
    auto now = std::chrono::steady_clock::now();
    int elapsed_ms = int(std::chrono::duration_cast<std::chrono::milliseconds>(now - countdown_start_time).count());
    if (elapsed_ms >= countdown_duration_ms) {
      snake = Snake(initial_snake_length);
      food.respawn();
      last_move_time = std::chrono::steady_clock::now();
      app_state = GameState::Playing;
    }
  }

  void update_playing() {
    if (is_action_pressed(key_bindings.pause)) { app_state = GameState::Pause; return; }
    if (is_action_down(key_bindings.up)) { snake.set_direction(Direction::Up); }
    else if (is_action_down(key_bindings.down)) { snake.set_direction(Direction::Down); }
    else if (is_action_down(key_bindings.left)) { snake.set_direction(Direction::Left); }
    else if (is_action_down(key_bindings.right)) { snake.set_direction(Direction::Right); }
    auto now = std::chrono::steady_clock::now();
    if (now - last_move_time >= std::chrono::milliseconds(tick_rate_ms)) {
      snake.update();
      last_move_time = now;
      Point head = snake.get_head();
      if (wrapping_enabled) {
        bool wrapped = false;
        if (head.x < 0) { head.x = GRID_WIDTH - 1; wrapped = true; }
        else if (head.x >= GRID_WIDTH) { head.x = 0; wrapped = true; }
        if (head.y < 0) { head.y = GRID_HEIGHT - 1; wrapped = true; }
        else if (head.y >= GRID_HEIGHT) { head.y = 0; wrapped = true; }
        if (wrapped) { snake.set_head(head); }
      } else {
        if (head.x < 0 || head.x >= GRID_WIDTH || head.y < 0 || head.y >= GRID_HEIGHT) { game_over(); return; }
      }
      if (snake.has_self_collision()) { game_over(); return; }
      if (head.x == food.get_position().x && head.y == food.get_position().y) {
        snake.grow(); food.respawn();
      }
    }
  }

void update_pause() {
    const int button_count = 4; // Resume, Settings, Restart, Main Menu
    const int spacing = 20;     // Vertical space between buttons
    const int total_button_height = button_count * BUTTON_HEIGHT + (button_count - 1) * spacing;
    const int start_x = SCREEN_WIDTH / 2 - BUTTON_WIDTH / 2;
    const int title_bottom_y = 80 + 60;
    const int available_height = SCREEN_HEIGHT - title_bottom_y - 20; // Leave some margin at the bottom
    const int start_y = title_bottom_y + (available_height - total_button_height) / 2 + 20;

    // Define button rectangles using calculated layout
    Rectangle resume_button    = { (float)start_x, (float)start_y,                                (float)BUTTON_WIDTH, (float)BUTTON_HEIGHT };
    Rectangle settings_button  = { (float)start_x, (float)(start_y + 1 * (BUTTON_HEIGHT + spacing)), (float)BUTTON_WIDTH, (float)BUTTON_HEIGHT };
    Rectangle restart_button   = { (float)start_x, (float)(start_y + 2 * (BUTTON_HEIGHT + spacing)), (float)BUTTON_WIDTH, (float)BUTTON_HEIGHT };
    Rectangle main_menu_button = { (float)start_x, (float)(start_y + 3 * (BUTTON_HEIGHT + spacing)), (float)BUTTON_WIDTH, (float)BUTTON_HEIGHT };

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
      if (is_mouse_in_rect(resume_button)) { app_state = GameState::Playing; }
      else if (is_mouse_in_rect(settings_button)) { previous_state = GameState::Pause; app_state = GameState::Settings; }
      else if (is_mouse_in_rect(restart_button)) { app_state = GameState::ConfirmRestart; }
      else if (is_mouse_in_rect(main_menu_button)) { app_state = GameState::ConfirmMainMenu; }
    }

    // Also allow resuming with the resume keybind
    if (is_action_pressed(key_bindings.resume)) {
      app_state = GameState::Playing;
    }
  }

  void update_confirm_restart() {
    Rectangle yes_button = { SCREEN_WIDTH/2 - BUTTON_WIDTH - 10,
                             SCREEN_HEIGHT/2 + 40,
                             BUTTON_WIDTH, BUTTON_HEIGHT };
    Rectangle no_button = { SCREEN_WIDTH/2 + 10,
                            SCREEN_HEIGHT/2 + 40,
                            BUTTON_WIDTH, BUTTON_HEIGHT };
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
      if (is_mouse_in_rect(yes_button)) {
        snake = Snake(initial_snake_length);
        food.respawn();
        last_move_time = std::chrono::steady_clock::now();
        app_state = GameState::Playing;
      } else if (is_mouse_in_rect(no_button)) { app_state = GameState::Pause; }
    }
  }

  void update_confirm_main_menu() {
    Rectangle yes_button = { SCREEN_WIDTH/2 - BUTTON_WIDTH - 10,
                             SCREEN_HEIGHT/2 + 40,
                             BUTTON_WIDTH, BUTTON_HEIGHT };
    Rectangle no_button = { SCREEN_WIDTH/2 + 10,
                            SCREEN_HEIGHT/2 + 40,
                            BUTTON_WIDTH, BUTTON_HEIGHT };
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
      if (is_mouse_in_rect(yes_button)) { app_state = GameState::StartMenu; }
      else if (is_mouse_in_rect(no_button)) { app_state = GameState::Pause; }
    }
  }

  void update_game_over() {
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { app_state = GameState::StartMenu; }
  }

  void game_over() {
    int current_length = snake.get_length();
    best_length = std::max(best_length, current_length);
    app_state = GameState::GameOver;
  }

  // Drawing functions
  void draw() {
    BeginDrawing();
    ClearBackground(RAYWHITE);
    switch (app_state) {
      case GameState::StartMenu:      draw_start_menu(); break;
      case GameState::Settings:       draw_settings(); break;
      case GameState::Keybinds:       draw_keybinds(); break;
      case GameState::Countdown:      draw_countdown(); break;
      case GameState::Playing:        draw_playing(); break;
      case GameState::Pause:          draw_pause(); break;
      case GameState::ConfirmRestart: draw_confirm_restart(); break;
      case GameState::ConfirmMainMenu:draw_confirm_main_menu(); break;
      case GameState::GameOver:       draw_game_over(); break;
    }
    EndDrawing();
  }

  void draw_start_menu() {
    int title_font_size = 60, subtitle_font_size = 20;
    std::string title_text = "SNAKEY", subtitle_text = "By: vs-123";
    int title_width = MeasureText(title_text.c_str(), title_font_size);
    int subtitle_width = MeasureText(subtitle_text.c_str(), subtitle_font_size);
    DrawText(title_text.c_str(), SCREEN_WIDTH/2 - title_width/2, 80, title_font_size, DARKBLUE);
    DrawText(subtitle_text.c_str(), SCREEN_WIDTH/2 - subtitle_width/2, 150, subtitle_font_size, DARKBLUE);

    const int button_count = 3;
    const int spacing = 20;
    int total_height = button_count * BUTTON_HEIGHT + (button_count - 1) * spacing;
    int start_y = (SCREEN_HEIGHT - total_height) / 2;
    int start_x = SCREEN_WIDTH / 2 - BUTTON_WIDTH / 2;
    Rectangle play_button = { (float)start_x, (float)start_y, (float)BUTTON_WIDTH, (float)BUTTON_HEIGHT };
    Rectangle settings_button = { (float)start_x, (float)(start_y + BUTTON_HEIGHT + spacing), (float)BUTTON_WIDTH, (float)BUTTON_HEIGHT };
    Rectangle quit_button = { (float)start_x, (float)(start_y + 2 * (BUTTON_HEIGHT + spacing)), (float)BUTTON_WIDTH, (float)BUTTON_HEIGHT };

    // Draw buttons
    DrawRectangleRec(play_button, get_button_color(play_button));
    DrawRectangleRec(settings_button, get_button_color(settings_button));
    DrawRectangleRec(quit_button, get_button_color(quit_button));

    // Center text in each button
    int font_size = 30;
    std::string play_str = "PLAY";
    int play_text_width = MeasureText(play_str.c_str(), font_size);
    int play_text_x = play_button.x + (play_button.width - play_text_width) / 2;
    int play_text_y = play_button.y + (play_button.height - font_size) / 2;
    DrawText(play_str.c_str(), play_text_x, play_text_y, font_size, BLACK);

    std::string settings_str = "SETTINGS";
    int settings_text_width = MeasureText(settings_str.c_str(), font_size);
    int settings_text_x = settings_button.x + (settings_button.width - settings_text_width) / 2;
    int settings_text_y = settings_button.y + (settings_button.height - font_size) / 2;
    DrawText(settings_str.c_str(), settings_text_x, settings_text_y, font_size, BLACK);

    std::string quit_str = "QUIT";
    int quit_text_width = MeasureText(quit_str.c_str(), font_size);
    int quit_text_x = quit_button.x + (quit_button.width - quit_text_width) / 2;
    int quit_text_y = quit_button.y + (quit_button.height - font_size) / 2;
    DrawText(quit_str.c_str(), quit_text_x, quit_text_y, font_size, BLACK);
  }

void draw_settings() {
    DrawText("SETTINGS", SCREEN_WIDTH/2 - MeasureText("SETTINGS", 40)/2, 20, 40, DARKBLUE);
    DrawText("INITIAL SNAKE LENGTH", 100, 110, 20, DARKGRAY);
    Rectangle snake_length_slider = { 100, 150, 200, 10 };
    DrawRectangleRec(snake_length_slider, LIGHTGRAY);
    float snake_length_ratio = (initial_snake_length - 1.0f) / 9.0f;
    Rectangle snake_length_knob = { snake_length_slider.x + snake_length_ratio * snake_length_slider.width - 5,
                                    snake_length_slider.y - 5, 10, 20 };
    DrawRectangleRec(snake_length_knob, DARKGRAY);
    std::string snake_length_str = std::to_string(initial_snake_length);
    DrawText(snake_length_str.c_str(), snake_length_slider.x + snake_length_slider.width + 20,
             snake_length_slider.y - 5, 20, DARKBLUE);

    DrawText("TICK RATE (ms)", 100, 210, 20, DARKGRAY);
    Rectangle tick_rate_slider = { 100, 250, 200, 10 };
    DrawRectangleRec(tick_rate_slider, LIGHTGRAY);
    float tick_rate_ratio = (tick_rate_ms - 50.0f) / 450.0f;
    Rectangle tick_rate_knob = { tick_rate_slider.x + tick_rate_ratio * tick_rate_slider.width - 5,
                                tick_rate_slider.y - 5, 10, 20 };
    DrawRectangleRec(tick_rate_knob, DARKGRAY);
    std::string tick_rate_str = std::to_string(tick_rate_ms);
    DrawText(tick_rate_str.c_str(), tick_rate_slider.x + tick_rate_slider.width + 20,
             tick_rate_slider.y - 5, 20, DARKBLUE);

    DrawText("WRAPPING", 140, 345, 20, DARKGRAY);
    Rectangle wrapping_checkbox = { 100, 345, 20, 20 };
    DrawRectangleRec(wrapping_checkbox, LIGHTGRAY);
    if (wrapping_enabled) {
      DrawLine(wrapping_checkbox.x, wrapping_checkbox.y,
               wrapping_checkbox.x + wrapping_checkbox.width,
               wrapping_checkbox.y + wrapping_checkbox.height, DARKBLUE);
      DrawLine(wrapping_checkbox.x, wrapping_checkbox.y + wrapping_checkbox.height,
               wrapping_checkbox.x + wrapping_checkbox.width, wrapping_checkbox.y, DARKBLUE);
    }
    Rectangle keybinds_button = { 100, 410, 200, 40 };
    DrawRectangleRec(keybinds_button, get_button_color(keybinds_button));

    // Center "KEYBINDS" text
    int keybinds_font_size = 30;
    const char* keybinds_text = "KEYBINDS";
    int keybinds_text_width = MeasureText(keybinds_text, keybinds_font_size);
    DrawText(keybinds_text, keybinds_button.x + (keybinds_button.width - keybinds_text_width) / 2,
             keybinds_button.y + (keybinds_button.height - keybinds_font_size) / 2, keybinds_font_size, BLACK);

    // Draw and Center "BACK" button text
    Rectangle back_button = { SCREEN_WIDTH - 120.0f, SCREEN_HEIGHT - 60.0f, 100.0f, 40.0f }; 
    DrawRectangleRec(back_button, get_button_color(back_button));

    int back_font_size = 30;
    const char* back_text = "BACK";
    int back_text_width = MeasureText(back_text, back_font_size);
    // Calculate centered position
    int back_text_x = back_button.x + (back_button.width - back_text_width) / 2;
    int back_text_y = back_button.y + (back_button.height - back_font_size) / 2;
    DrawText(back_text, back_text_x, back_text_y, back_font_size, BLACK); 
  }

  void draw_keybinds() {
    DrawText("KEYBINDS", SCREEN_WIDTH/2 - MeasureText("KEYBINDS", 40)/2, 20, 40, DARKBLUE);
    int start_y = 100, spacing = 50, start_x = 100, entry_width = 400;
    const char* actions[6] = { "PAUSE", "RESUME", "UP", "DOWN", "LEFT", "RIGHT" };
    for (int i = 0; i < 6; ++i) {
      Rectangle entry_rect = { float(start_x), float(start_y + i * spacing), float(entry_width), 40 };
      DrawRectangleRec(entry_rect, LIGHTGRAY);
      DrawText(actions[i], entry_rect.x + 10, entry_rect.y + 5, 20, DARKBLUE);
      std::string key_str;
      int key_code = 0;
      switch (i) {
        case 0: key_code = key_bindings.pause[0]; break;
        case 1: key_code = key_bindings.resume[0]; break;
        case 2: key_code = key_bindings.up[0]; break;
        case 3: key_code = key_bindings.down[0]; break;
        case 4: key_code = key_bindings.left[0]; break;
        case 5: key_code = key_bindings.right[0]; break;
      }
      key_str = key_code_to_string(key_code);
      DrawText(key_str.c_str(), entry_rect.x + 250, entry_rect.y + 5, 20, MAROON);
      if (current_edit_action == i) {
        DrawRectangleLines(int(entry_rect.x), int(entry_rect.y), int(entry_rect.width), int(entry_rect.height), RED);
      }
    }
    Rectangle back_button = { SCREEN_WIDTH - 120, SCREEN_HEIGHT - 60, 100, 40 };
    DrawRectangleRec(back_button, get_button_color(back_button));
    DrawText("BACK", back_button.x + 20, back_button.y + 10, 30, BLACK);
  }

  void draw_countdown() {
    auto now = std::chrono::steady_clock::now();
    int elapsed_ms = int(std::chrono::duration_cast<std::chrono::milliseconds>(now - countdown_start_time).count());
    int remaining_secs = std::max(0, (countdown_duration_ms - elapsed_ms) / 1000);
    std::string countdown_text = "Starting in " + std::to_string(remaining_secs + 1) + "...";
    int text_width = MeasureText(countdown_text.c_str(), 40);
    DrawText(countdown_text.c_str(), SCREEN_WIDTH/2 - text_width/2, SCREEN_HEIGHT/2 - 20, 40, DARKBLUE);
  }

  void draw_playing() {
    food.draw();
    snake.draw();
  }

void draw_pause() {
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(RAYWHITE, 0.8f));

    // Draw "PAUSED" title
    int title_font_size = 60;
    const char* title_text = "PAUSED";
    int title_width = MeasureText(title_text, title_font_size);
    DrawText(title_text, SCREEN_WIDTH/2 - title_width/2, 80, title_font_size, DARKBLUE);

    // Calculate Button Layout
    const int button_count = 4; // Resume, Settings, Restart, Main Menu
    const int spacing = 20;     // Vertical space between buttons
    const int total_button_height = button_count * BUTTON_HEIGHT + (button_count - 1) * spacing;
    const int start_x = SCREEN_WIDTH / 2 - BUTTON_WIDTH / 2;
    const int title_bottom_y = 80 + title_font_size;
    const int available_height = SCREEN_HEIGHT - title_bottom_y - 20; // Leave some margin at the bottom
    const int start_y = title_bottom_y + (available_height - total_button_height) / 2 + 20;

    // Define button rectangles using calculated layout
    Rectangle resume_button    = { (float)start_x, (float)start_y,                                (float)BUTTON_WIDTH, (float)BUTTON_HEIGHT };
    Rectangle settings_button  = { (float)start_x, (float)(start_y + 1 * (BUTTON_HEIGHT + spacing)), (float)BUTTON_WIDTH, (float)BUTTON_HEIGHT };
    Rectangle restart_button   = { (float)start_x, (float)(start_y + 2 * (BUTTON_HEIGHT + spacing)), (float)BUTTON_WIDTH, (float)BUTTON_HEIGHT };
    Rectangle main_menu_button = { (float)start_x, (float)(start_y + 3 * (BUTTON_HEIGHT + spacing)), (float)BUTTON_WIDTH, (float)BUTTON_HEIGHT };

    // Draw Buttons and Text
    int button_font_size = 30;
    const char* resume_text = "RESUME";
    const char* settings_text = "SETTINGS";
    const char* restart_text = "RESTART";
    const char* main_menu_text = "MAIN MENU";

    DrawRectangleRec(resume_button, get_button_color(resume_button));
    DrawText(resume_text, resume_button.x + (BUTTON_WIDTH - MeasureText(resume_text, button_font_size)) / 2, resume_button.y + (BUTTON_HEIGHT - button_font_size) / 2, button_font_size, BLACK);

    DrawRectangleRec(settings_button, get_button_color(settings_button));
    DrawText(settings_text, settings_button.x + (BUTTON_WIDTH - MeasureText(settings_text, button_font_size)) / 2, settings_button.y + (BUTTON_HEIGHT - button_font_size) / 2, button_font_size, BLACK);

    DrawRectangleRec(restart_button, get_button_color(restart_button));
    DrawText(restart_text, restart_button.x + (BUTTON_WIDTH - MeasureText(restart_text, button_font_size)) / 2, restart_button.y + (BUTTON_HEIGHT - button_font_size) / 2, button_font_size, BLACK);

    DrawRectangleRec(main_menu_button, get_button_color(main_menu_button));
    DrawText(main_menu_text, main_menu_button.x + (BUTTON_WIDTH - MeasureText(main_menu_text, button_font_size)) / 2, main_menu_button.y + (BUTTON_HEIGHT - button_font_size) / 2, button_font_size, BLACK);
  }

  void draw_confirm_restart() {
    DrawText("Restart game?", SCREEN_WIDTH/2 - MeasureText("Restart game?", 40)/2, 100, 40, MAROON);
    Rectangle yes_button = { SCREEN_WIDTH/2 - BUTTON_WIDTH - 10,
                             SCREEN_HEIGHT/2 + 40,
                             BUTTON_WIDTH, BUTTON_HEIGHT };
    Rectangle no_button = { SCREEN_WIDTH/2 + 10,
                            SCREEN_HEIGHT/2 + 40,
                            BUTTON_WIDTH, BUTTON_HEIGHT };
    DrawRectangleRec(yes_button, get_button_color(yes_button));
    DrawText("YES", yes_button.x + (BUTTON_WIDTH - MeasureText("YES", 30)) / 2, yes_button.y + (BUTTON_HEIGHT - 30)/2, 30, BLACK);
    DrawRectangleRec(no_button, get_button_color(no_button));
    DrawText("NO", no_button.x + (BUTTON_WIDTH - MeasureText("NO", 30)) / 2, no_button.y + (BUTTON_HEIGHT - 30)/2, 30, BLACK);
  }

  void draw_confirm_main_menu() {
    DrawText("Return to Main Menu?", SCREEN_WIDTH/2 - MeasureText("Return to Main Menu?", 40)/2, 100, 40, MAROON);
    Rectangle yes_button = { SCREEN_WIDTH/2 - BUTTON_WIDTH - 10,
                             SCREEN_HEIGHT/2 + 40,
                             BUTTON_WIDTH, BUTTON_HEIGHT };
    Rectangle no_button = { SCREEN_WIDTH/2 + 10,
                            SCREEN_HEIGHT/2 + 40,
                            BUTTON_WIDTH, BUTTON_HEIGHT };
    DrawRectangleRec(yes_button, get_button_color(yes_button));
    DrawText("YES", yes_button.x + (BUTTON_WIDTH - MeasureText("YES", 30)) / 2, yes_button.y + (BUTTON_HEIGHT - 30)/2, 30, BLACK);
    DrawRectangleRec(no_button, get_button_color(no_button));
    DrawText("NO", no_button.x + (BUTTON_WIDTH - MeasureText("NO", 30)) / 2, no_button.y + (BUTTON_HEIGHT - 30)/2, 30, BLACK);
  }

  void draw_game_over() {
    std::string game_over_text = "GAME OVER";
    int game_over_width = MeasureText(game_over_text.c_str(), 60);
    DrawText(game_over_text.c_str(), SCREEN_WIDTH/2 - game_over_width/2, 100, 60, MAROON);
    std::string last_length = "Length: " + std::to_string(snake.get_length());
    int last_length_width = MeasureText(last_length.c_str(), 30);
    DrawText(last_length.c_str(), SCREEN_WIDTH/2 - last_length_width/2, 200, 30, DARKBLUE);
    std::string best_length_str = "BEST LENGTH: " + std::to_string(best_length);
    int best_length_width = MeasureText(best_length_str.c_str(), 30);
    DrawText(best_length_str.c_str(), SCREEN_WIDTH/2 - best_length_width/2, 250, 30, DARKBLUE);
    DrawText("Click anywhere to return", SCREEN_WIDTH/2 - MeasureText("Click anywhere to return", 20)/2, 350, 20, DARKGRAY);
  }
};

int main() {
  Game game;
  game.run();
  return 0;
}
