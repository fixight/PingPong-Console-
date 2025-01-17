#include <iostream>
#include <memory>
#include <conio.h>
#include <chrono>
#include <thread>
#include <vector>
#include <string>
#include <random>
#include <mutex>
#include <algorithm>

struct Position {
    int x;
    int y;

    Position() : x(0), y(0) {}
    Position(int xValue, int yValue) : x(xValue), y(yValue) {}
};

constexpr int MAP_WIDTH = 25;
constexpr int MAP_HEIGHT = 15;
constexpr int PADDLE_SIZE = 3;

class GameManager {
public:

    static GameManager* getInstance() {
        std::call_once(onceFlag_, []() {
            instance_ = new GameManager();
        });
        return instance_;
    }

    GameManager(const GameManager&) = delete;
    GameManager& operator=(const GameManager&) = delete;

public:
    void ResetRound(int scoringPlayer)
    {
        std::cout << "Raund over. Point for player " << scoringPlayer + 1 << "!" << std::endl;
    }

    int GetScorePlayer1() const { return ScorePlayer1; }
    int GetScorePlayer2() const { return ScorePlayer2; }

private:
    GameManager();

    int ScorePlayer1 = 0;

    int ScorePlayer2 = 0;

    friend class Ball;

    static GameManager* instance_;
    static std::once_flag onceFlag_;
};

class Ball
{
public:
    Ball() = default;

    Position BallLocation = Position(MAP_WIDTH / 2, MAP_HEIGHT / 2);
    Position BallDirection = GetRandomDirection();

    void Move(const std::vector<std::string>& Map) {
        moveIntervalCounter_++;
        if (moveIntervalCounter_ >= moveInterval_) {
            int nextX = BallLocation.x + BallDirection.x;
            int nextY = BallLocation.y + BallDirection.y;

            if (nextX == 0) {
                GameManager::getInstance()->ResetRound(1);
                GameManager::getInstance()->ScorePlayer2++;
                ResetWithDelay();
                return;
            }
            if (nextX == MAP_WIDTH - 1) {
                GameManager::getInstance()->ResetRound(0);
                GameManager::getInstance()->ScorePlayer1++;
                ResetWithDelay();
                return;
            }

            if (nextX >= 0 && nextX < MAP_WIDTH && nextY >= 0 && nextY < MAP_HEIGHT) {
                if (Map[nextY][nextX * 2] == ' ') {
                    BallLocation.x = nextX;
                    BallLocation.y = nextY;

                } else {
                    Hit(Map, nextX, nextY);
                    BallLocation.x += BallDirection.x;
                    BallLocation.y += BallDirection.y;
                }
            } else {
                HandleBoundaryCollision();
            }
            moveIntervalCounter_ = 0;
        }
    }

    bool CheckGoal() const {
        return false;
    }

    void Reset() {
        BallLocation = Position(MAP_WIDTH / 2, MAP_HEIGHT / 2);
        BallDirection = GetRandomDirection();
    }

    void ResetWithDelay() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        Reset();
    }

    void SetMoveInterval(int interval) {
        moveInterval_ = interval;
    }

private:
    int moveIntervalCounter_ = 0;
    int moveInterval_ = 2;

    static Position GetRandomDirection() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> sign_distrib(0, 1);

        int dx = (sign_distrib(gen) == 0) ? -1 : 1;
        int dy = (sign_distrib(gen) == 0) ? -1 : 1;
        return {dx, dy};
    }

    void Hit(const std::vector<std::string>& Map, int x, int y) {
        auto normal = GetNormal(Map, x, y);
        if (normal.first == 0 && normal.second == 0) return;

        int dotProduct = BallDirection.x * normal.first + BallDirection.y * normal.second;
        int reflectedX = BallDirection.x - 2 * dotProduct * normal.first;
        int reflectedY = BallDirection.y - 2 * dotProduct * normal.second;

        BallDirection.x = ClampToDiscrete(reflectedX);
        BallDirection.y = ClampToDiscrete(reflectedY);
    }

    std::pair<int, int> GetNormal(const std::vector<std::string>& Map, int x, int y) {
        if (x < 0 || y < 0 || x >= MAP_WIDTH || y >= MAP_HEIGHT) return {0, 0};

        if (x == 1 && Map[y][x * 2] == '|') return {1, 0};
        if (x == MAP_WIDTH - 2 && Map[y][x * 2] == '|') return {-1, 0};

        if (Map[y][x * 2] == '*') {
            if (y > 0 && Map[y - 1][x * 2] == ' ') return {0, -1};
            if (x < MAP_WIDTH - 1 && Map[y][(x + 1) * 2] == ' ') return {1, 0};
            if (y < MAP_HEIGHT - 1 && Map[y + 1][x * 2] == ' ') return {0, 1};
            if (x > 0 && Map[y][(x - 1) * 2] == ' ') return {-1, 0};

            if (y > 0 && x > 0 && Map[y - 1][(x - 1) * 2] == ' ') return {-1, -1};
            if (y > 0 && x < MAP_WIDTH - 1 && Map[y - 1][(x + 1) * 2] == ' ') return {1, -1};
            if (y < MAP_HEIGHT - 1 && x > 0 && Map[y + 1][(x - 1) * 2] == ' ') return {-1, 1};
            if (y < MAP_HEIGHT - 1 && x < MAP_WIDTH - 1 && Map[y + 1][(x + 1) * 2] == ' ') return {1, 1};
        }
        return {0, 0};
    }

    int ClampToDiscrete(int value) {
        if (value > 0) return 1;
        if (value < 0) return -1;
        return 0;
    }

    void HandleBoundaryCollision() {
        if (BallLocation.y + BallDirection.y < 0 || BallLocation.y + BallDirection.y >= MAP_HEIGHT) {
            BallDirection.y = -BallDirection.y;
        }
    }
};

class Controller {
public:
    Controller() = default;
    virtual ~Controller() = default;
    virtual void Move() = 0;

    void SetPaddleLocation(int x, int y) {
        PaddleLocation.y = std::max(1, std::min(y, MAP_HEIGHT - PADDLE_SIZE));
        PaddleLocation.x = x;
    }

    Position GetPaddleLocation() const { return PaddleLocation; }

protected:
    Position PaddleLocation;
};

class AiController : public Controller {
public:
    AiController(Ball* ball) : ball_(ball) {}
    void Move() override {
        if (ball_) {
            int ballY = ball_->BallLocation.y;
            int paddleCenterY = PaddleLocation.y + PADDLE_SIZE / 2;

            if (ballY < paddleCenterY) {
                if (PaddleLocation.y > 1) {
                    PaddleLocation.y -= AI_SPEED;
                }
            } else if (ballY > paddleCenterY) {
                if (PaddleLocation.y < MAP_HEIGHT - PADDLE_SIZE) {
                    PaddleLocation.y += AI_SPEED;
                }
            }
        }
    }

private:
    Ball* ball_;
    static constexpr int AI_SPEED = 1;
};

class PlayerController : public Controller {
    void Move() override {
        while (_kbhit()) {
            char input = _getch();
            if (input == 'W' || input == 'w') {
                if (PaddleLocation.y > 1)
                    PaddleLocation.y -= 1;
            } else if (input == 'S' || input == 's') {
                if (PaddleLocation.y < MAP_HEIGHT - PADDLE_SIZE)
                    PaddleLocation.y += 1;
            }
        }
    }
};

void InitGame();



GameManager* GameManager::instance_ = nullptr;
std::once_flag GameManager::onceFlag_;
GameManager::GameManager() {}

void StartGame(std::shared_ptr<Controller> Player1, std::shared_ptr<Controller> Player2, std::shared_ptr<Ball> Ball);
void BuildMap(std::shared_ptr<Controller> Player1, std::shared_ptr<Controller> Player2, std::shared_ptr<Ball> Ball);
std::shared_ptr<Controller> CreateController(bool IsAiController, Ball* ball);
std::vector<std::string> GetMapForCollision(std::shared_ptr<Controller> Player1, std::shared_ptr<Controller> Player2);

int main() {
    InitGame();
    return 0;
}

void InitGame() {
    std::shared_ptr<Controller> player1;
    std::shared_ptr<Controller> player2;
    std::shared_ptr<Ball> ball = std::make_shared<Ball>();
    ball->SetMoveInterval(2);

    std::cout << "Is Player 1 AI? Enter 1 for yes, 0 for no: ";
    int input;
    std::cin >> input;
    player1 = CreateController(input, ball.get());
    player1->SetPaddleLocation(1, MAP_HEIGHT / 2 - PADDLE_SIZE / 2);

    std::cout << "Is Player 2 AI? Enter 1 for yes, 0 for no: ";
    std::cin >> input;
    player2 = CreateController(input, ball.get());
    player2->SetPaddleLocation(MAP_WIDTH - 2, MAP_HEIGHT / 2 - PADDLE_SIZE / 2);

    StartGame(player1, player2, ball);
}

void StartGame(std::shared_ptr<Controller> Player1, std::shared_ptr<Controller> Player2, std::shared_ptr<Ball> Ball) {
    auto lastUpdateTime = std::chrono::high_resolution_clock::now();

    while (true) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        auto deltaTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastUpdateTime);

        if (deltaTime.count() >= 66) {
            Player1->Move();
            Player2->Move();
            Ball->Move(GetMapForCollision(Player1, Player2));

            BuildMap(Player1, Player2, Ball);
            lastUpdateTime = currentTime;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

std::vector<std::string> GetMapForCollision(std::shared_ptr<Controller> Player1, std::shared_ptr<Controller> Player2) {
    std::vector<std::string> map(MAP_HEIGHT, std::string(MAP_WIDTH * 2, ' '));
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (x == 0 || x == MAP_WIDTH - 1 || y == 0 || y == MAP_HEIGHT - 1) {
                map[y][x * 2] = '*';
            }
        }
    }
    for (int i = 0; i < PADDLE_SIZE; ++i) {
        if (Player1->GetPaddleLocation().y + i >= 0 && Player1->GetPaddleLocation().y + i < MAP_HEIGHT)
            map[Player1->GetPaddleLocation().y + i][Player1->GetPaddleLocation().x * 2] = '|';
        if (Player2->GetPaddleLocation().y + i >= 0 && Player2->GetPaddleLocation().y + i < MAP_HEIGHT)
            map[Player2->GetPaddleLocation().y + i][Player2->GetPaddleLocation().x * 2] = '|';
    }
    return map;
}

void BuildMap(std::shared_ptr<Controller> Player1, std::shared_ptr<Controller> Player2, std::shared_ptr<Ball> Ball) {
    std::vector<std::string> buffer(MAP_HEIGHT, std::string(MAP_WIDTH * 2, ' '));

    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (x == 0 || x == MAP_WIDTH - 1 || y == 0 || y == MAP_HEIGHT - 1) {
                buffer[y][x * 2] = '*';
            }
        }
    }

    for (int i = 0; i < PADDLE_SIZE; ++i) {
        if (Player1->GetPaddleLocation().y + i >= 0 && Player1->GetPaddleLocation().y + i < MAP_HEIGHT)
            buffer[Player1->GetPaddleLocation().y + i][Player1->GetPaddleLocation().x * 2] = '|';
        if (Player2->GetPaddleLocation().y + i >= 0 && Player2->GetPaddleLocation().y + i < MAP_HEIGHT)
            buffer[Player2->GetPaddleLocation().y + i][Player2->GetPaddleLocation().x * 2] = '|';
    }
    buffer[Ball->BallLocation.y][Ball->BallLocation.x * 2] = 'O';

    system("cls");
    for (const auto& line : buffer) {
        std::cout << line << '\n';
    }
    std::cout << "Score: Player 1 - " << GameManager::getInstance()->GetScorePlayer1()
              << ", Player 2 - " << GameManager::getInstance()->GetScorePlayer2() << std::endl;
}

std::shared_ptr<Controller> CreateController(bool IsAiController, Ball* ball) {
    return IsAiController
        ? std::static_pointer_cast<Controller>(std::make_shared<AiController>(ball))
        : std::static_pointer_cast<Controller>(std::make_shared<PlayerController>());
}