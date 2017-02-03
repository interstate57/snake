#include "game.h"

#include <set>


const int START_X_0 = 5;
const int START_Y_0 = 5;
const int START_X_1 = 5;
const int START_Y_1 = 15;
const int START_X_2 = 15;
const int START_Y_2 = 15;
const int START_X_3 = 15;
const int START_Y_3 = 5;

const double CHERRY_PROBABILITY = 0.1;
const double LIFE_PROBABILITY = 0.01;
const double BOMB_PROBABILITY = 0.05;

const int CHERRY_LIFETIME = 20;
const int LIFE_LIFETIME = 25;
const int BOMB_LIFETIME = 15;

const int CHERRY_LENGTH_BONUS = 2;
const int CHERRY_SCORE_BONUS = 10;

const double EXPECTED_CHERRIES = 3;
const double EXPECTED_LIVES = 0.5;
const double EXPECTED_BOMBS = 0.5;

const int START_LIVES = 1;

const int MAX_MOVES = 100000;


bool randomEvent(double probability);


// --- Remove me
template<class T>
std::ostream& operator<<(std::ostream& os, const std::set<T>& v)
{
    for (const T& item: v) {
        os << item << " ";
    }
    os << std::endl;
    return os;
}

std::ostream& operator<<(std::ostream& os, const Point& p)
{
    os << "(" << p.x() << ", " << p.y() << ")";
    return os;
}

template<class T>
std::ostream& operator<<(std::ostream& os, std::vector<T>& v)
{
    for (const T& item: v) {
        os << item << " ";
    }
    os << std::endl;
    return os;
}
// --- \Remove me

PlayerState::PlayerState(Player * p, Snake s)
: player(p)
, score(0)
, lives(START_LIVES)
, snake(s)
{
}

Game::Game(
    const vector<vector<FieldType>>& pattern,
    Player * first,
    Player * second,
    Player * third,
    Player * fourth)
: pattern_(pattern)
, cherries_()
, lives_()
, deaths_()
, playerStates_(defaultPlayerStates(first, second, third, fourth))
{
    //srand(time(NULL));
    srand(57); // Let the random be determined in beta version.

    Field field = currentField();

    // FIXME: Rewrite field generation

    // Generate cherries, if any
    int attempts = floor(EXPECTED_CHERRIES / CHERRY_PROBABILITY);
    for (int i = 0; i < attempts; ++i) {
        if (randomEvent(CHERRY_PROBABILITY)) {
            boost::optional<Point> point = locateFieldObject();
            if (!point) // could not find a free cell
                break;

            cherries_.push_back(FieldObject(CHERRY_LIFETIME, *point));
        }
    }

    // Generate lives, if any
    attempts = floor(EXPECTED_LIVES / LIFE_PROBABILITY);
    for (int i = 0; i < attempts; ++i) {
        if (randomEvent(LIFE_PROBABILITY)) {
            boost::optional<Point> point = locateFieldObject();
            if (!point) // could not find a free cell
                break;

            lives_.push_back(FieldObject(LIFE_LIFETIME, *point));
        }
    }

    // Generate bombs, if any
    attempts = floor(EXPECTED_BOMBS / BOMB_PROBABILITY);
    for (int i = 0; i < attempts; ++i) {
        if (randomEvent(BOMB_PROBABILITY)) {
            boost::optional<Point> point = locateFieldObject();
            if (!point) // could not find a free cell
                break;

            deaths_.push_back(FieldObject(BOMB_LIFETIME, *point));
        }
    }
}

void Game::move()
{
    // 1. Generate new field
    Field field = currentField();

    // 2. Pass the field to the rest of the players. Wait for their turns.
    std::map<int, Direction> turns;
    for (auto playerState: playerStates_) {
        if (playerState.second.isAlive()) {
            turns[playerState.first] = playerState.second.player->makeTurn(field);
        }
    }

    // Find snakes that will die this turn.
    std::set<int> corpses;

    // Find new snake heads. Refresh players' snakes.
    std::map<int, Point> heads;
    for (auto turn: turns) {
        Snake& snake = playerStates_[turn.first].snake;
        snake.move(turn.second);
        heads[turn.first] = snake.head();
    }

    // Now snakes are already new, and the field is old.
    for (auto item: snakes()) {
        // New field without current snake
        Field newField = currentField(item.first);
        Point head = item.second.head();

        // Does the snake leave the board?
        if (head.x() < 0 ||
            head.x() >= width() ||
            head.y() < 0 ||
            head.y() >= height()) {

            corpses.insert(item.first);
            continue;
        }

        if (field.at(head.x(), head.y()) == BOMB ||
            field.at(head.x(), head.y()) == WALL ||
            // If at next turn there is a snake here, then either it came here now or it was here before.
            // If it came here, then it's a head (and our snake is a corpse already).
            // If it was here earlier, then it's the body present last time.
            newField.at(head.x(), head.y()) == SNAKE) {

            corpses.insert(item.first);

            // The bomb that exploded must disappear.
            if (field.at(head.x(), head.y()) == BOMB) {
                removeFieldObject(deaths_, head);
            }
        }

        if (field.at(head.x(), head.y()) == CHERRY) { // eat
            playerStates_[item.first].snake.increase(CHERRY_LENGTH_BONUS);
            playerStates_[item.first].score += CHERRY_SCORE_BONUS;

            // remove the cherry if we've eaten it
            removeFieldObject(cherries_, head);
        }

        else if (field.at(head.x(), head.y()) == LIFE) { // no support for lives at the moment
            // do nothing
            removeFieldObject(lives_, head);
        }
    }

    // Remove the dead.
    for (int player: corpses) {
        --playerStates_[player].lives;
    }

#ifdef DEBUG
    std::cout << "Corpses this turn: " << corpses << std::endl;
#endif

    // 3. Kill the expired stuff. Generate new stuff in free places.
    refreshFieldObjects(cherries_, CHERRY_PROBABILITY, CHERRY_LIFETIME);
    refreshFieldObjects(lives_, LIFE_PROBABILITY, LIFE_LIFETIME);
    refreshFieldObjects(deaths_, BOMB_PROBABILITY, BOMB_LIFETIME);
}

void Game::print() const
{
    std::cout << currentField() << std::endl;
}

bool Game::isOver() const
{
    return std::all_of(
        playerStates_.begin(),
        playerStates_.end(),
        [] (std::pair<int, PlayerState> p)
        {
            return !p.second.isAlive();
        }
    );
}

bool randomEvent(double prob)
{
    double result = rand() * 1.0 / RAND_MAX;
    return result <= prob;
}

boost::optional<Point> Game::locateFieldObject() const
{
    Field field = currentField();

    int attempts = 0;
    const int MAX_ATTEMPTS = 1000;
    while (attempts < MAX_ATTEMPTS) {
        int x = rand() % width();
        int y = rand() % height();
        if (field.at(x, y) == NOTHING)
            return Point(x, y);
    }
    return boost::none;
}

std::map<int, Snake> Game::snakes(int except) const
{
    std::map<int, Snake> result;
    for (auto playerState: playerStates_) {
        if (playerState.first == except)
            continue;
        if (playerState.second.isAlive()) {
            result[playerState.first] = playerState.second.snake;
        }
    }
    return result;
}

Field Game::currentField(int except) const
{
    return Field(pattern_, snakes(except), cherries_, lives_, deaths_);
}

std::map<int, PlayerState> Game::defaultPlayerStates(
    Player * first, Player * second, Player * third, Player * fourth)
{
    map<int, PlayerState> result;
    result[0] = PlayerState(
        first,
        Snake(
            Point(START_X_0, START_Y_0),
            {RIGHT, RIGHT, RIGHT}
        )
    );
    result[1] = PlayerState(
        second,
        Snake(
            Point(START_X_1, START_Y_1),
            {RIGHT, RIGHT, RIGHT}
        )
    );
    result[2] = PlayerState(
        third,
        Snake(
            Point(START_X_2, START_Y_2),
            {LEFT, LEFT, LEFT}
        )
    );
    result[3] = PlayerState(
        fourth,
        Snake(
            Point(START_X_3, START_Y_3),
            {LEFT, LEFT, LEFT}
        )
    );
    return result;
}

void Game::refreshFieldObjects(std::vector<FieldObject>& fieldObjects,
                               double appearProbability,
                               int lifetime)
{
    // Remove expired stuff
    std::for_each(
        fieldObjects.begin(),
        fieldObjects.end(),
        [] (FieldObject& obj)
        {
            obj.tick();
        }
    );
    auto last = std::remove_if(
        fieldObjects.begin(),
        fieldObjects.end(),
        [] (const FieldObject& obj)
        {
            return !obj.isAlive();
        }
    );
    fieldObjects.resize(
        std::distance(
            fieldObjects.begin(), last
        )
    );
    // Generate new stuff
    if (randomEvent(appearProbability)) {
        boost::optional<Point> point = locateFieldObject();
        if (point) { // could not find a free cell
            fieldObjects.push_back(FieldObject(lifetime, *point));
        }
    }
}

void Game::removeFieldObject(std::vector<FieldObject>& fieldObjects, const Point& location)
{
    auto result = std::remove_if(fieldObjects.begin(),
                   fieldObjects.end(),
                   [location] (const FieldObject& obj)
                   {
                       return obj.point() == location;
                   }
    );
    fieldObjects.resize(std::distance(fieldObjects.begin(), result));
}