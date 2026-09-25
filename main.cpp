#include "helper.hpp"

#include <algorithm>
#include <random>

#include <cstring>

using namespace std;
using namespace unswbc;

#define KING 67
#define BOMB 69

auto directions = Direction::get_direction_list();
int state = 0;
bool seen[7][7] = {};


/* Strategies DFS Edition */

// True if can kill - BOMBER
bool kill_depth_dfs(const vector<Tile> &tiles, int depth,
    int max_depth, int row, int col, char team, vector<Direction> &move);

// Best starting move for pearl -  DEFAULT
int depth_dfs(const vector<Tile> &tiles, int depth, int max_depth, int row,
    int col, int first_move, vector<int> &valid_moves);

// True if move is dangerous - DEFAULT + KING
bool danger_dfs(const vector<Tile> &tiles, int depth, int max_depth, int row,
    int col, char team);

// Check seen array for most open starting move - KING
void open_dfs(const vector<Tile> &tiles, int depth, int max_depth, int row,
    int col, int first_move);

// Shout - All len == 2 in range 7 die on their turn


/* MAIN */

int main() {
    auto [ct, game] = init();

    // Shuffle Directions to randomise
    random_device rd;
    mt19937 shuffler(rd());
    // shuffle(directions.begin(), directions.end(), shuffler);

    while (update(ct, game)) {
        auto const here = ct.get_position();
        auto const* hereTile = ct.get_tile(here);
        auto const& tiles = ct.get_tiles();

        memset(seen, false, sizeof(seen));

        bool moved = false;

        // If a dragon is a BOMBER, then run the strategy where the dragon
        // will search for a enemey head in range and then kill them by sprinting
        if (state == BOMB) {
            // Depth based DFS to get the closest enemy head in range
            // increase depth from 1 to sprint max
            for (int max_depth = 1; max_depth <= min(3, ct.get_length()); max_depth++) {
                vector<Direction> move = {};
                if (kill_depth_dfs(tiles, 0, max_depth, 3, 3, ct.get_team().value, move)) {
                    ct.make_moves(move);
                    moved = true;
                    break;
                }
            }
        }

        vector<vector<int>> dir = {{-1, 0}, {0, 1}, {1, 0}, {0, -1}};
        vector<int> valid_moves(4, 1);

        if (state == 0 || state == KING) {
            for (int max_depth = 1; max_depth <= 3; max_depth++) {
                for (int i = 0; i < 4; i++) {
                    valid_moves[i] = !danger_dfs(tiles, 0, max_depth, 3+dir[i][0], 3+dir[i][1], ct.get_team().value);
                }
            }
            memset(seen, false, sizeof(seen));
        }

        // Default strategy, get the closest pearl
        // Depth based DFS to get the closest pearl
        // increase depth from 1 to 15 until a pearl is found
        if (!moved) {
            for (int max_depth = 1; max_depth < 15; max_depth++) {
                int move = depth_dfs(tiles, 0, max_depth, 3, 3, -1, valid_moves);
                if (move != -1) {
                    ct.make_move(directions[move]);
                    moved = true;
                    break;
                }
            }
        }

        // Fallback: If there is no pearl then move in the first valid direction
        if (!moved) {
            for (int i = 0; i < 4; i++) {
                if (!valid_moves[i]) continue;
                Direction direction = directions[i];

                // Check that no wall is blocking you from moving to next tile
                if (hereTile && !hereTile->get_edge(direction).is_passable()) {
                    continue;
                }

                // Check that no dragon is in the next tile
                auto const* ahead = ct.get_tile(here.add_dir(direction));
                if (ahead && ahead->get_dragon()) continue;

                ct.output_log("Moving in", direction);
                ct.make_move(direction);
                moved = true;
                break;
            }
        }

        // For dragons longer than 11, they have a 5% chance to split instead
        // of moving (to increase population)
        mt19937 gen(rd());
        uniform_int_distribution<int> distrib(1, 20);
        if (distrib(gen) == 1 && ct.get_length() >= 12 && ct.can_split(2)) {
            ct.do_split(2);
            moved = true;
        }

        // Fallback: If there is no valid movements, then attempt to split,
        // if splitting also fails then die. (Splitting allow us to recover part
        // of a dead dragon)
        if (!moved) {
            if (ct.can_split(ct.get_length()-2)) {
                ct.do_split(ct.get_length()-2);
            } else {
                ct.make_move(directions[0]);
            }
        }

        // If a dragon is over 11, it becomes a KING
        if (ct.get_length() == 12) state = KING;

        // A KING dragon and BOMBER dragons constantly convert all normal
        // dragons inrange into a BOMBER using sonar
        if (state == KING || state == BOMB) {
            for (auto const direction : directions) {
                ct.send_sonar(direction, BOMB);
            }
        }

        // A normal dragon if smaller than 6, will become a BOMBER on infection
        if (state == 0 && ct.get_length() <= 5) {
            for (auto const message : ct.get_sonar_messages()) {
                state = BOMB;
            }
        }

        end_turn();
    }
}


// Depth based DFS to get the closest enemy head in sprinting attack range
bool kill_depth_dfs(const vector<Tile> &tiles, int depth,
    int max_depth, int row, int col, char team, vector<Direction> &move
) {
    auto const& curr = tiles[row * 7 + col];

    // If its an enemy head
    if (tiles[row * 7 + col].get_dragon() && tiles[row * 7 + col].get_dragon()->is_dragon_head && tiles[row * 7 + col].get_dragon()->get_team().value != team) return true;

    // Nothing found in max range
    if (depth == max_depth) {
        return false;
    }

    seen[row][col] = true;

    for (int i = 0; i < 4; i++) {
        auto direction = directions[i];

        pair<int, int> offset = direction.get_offset();
        int nr = row + offset.second;
        int nc = col + offset.first;

        // Inbounds of grid, not visited and no kelp blocking
        if ((nr < 0 || nr >= 7 || nc < 0 || nc >= 7) ||
            seen[nr][nc] || !curr.get_edge(direction).is_passable()
        ) {
            continue;
        }

        // Check that no dragon body in this tile (we can't attack the body)
        if (tiles[nr * 7 + nc].get_dragon() && !tiles[nr * 7 + nc].get_dragon()->is_dragon_head) continue;

        move.push_back(direction);

        if (kill_depth_dfs(tiles, depth+1, max_depth, nr, nc, team, move)) {
            seen[row][col] = false;
            return true;
        }

        move.pop_back();
    }

    seen[row][col] = false;
    return false;
}


// Depth based DFS to get the closest pearl
// Return the direction that finds a pearl first
int depth_dfs(const vector<Tile> &tiles, int depth,
    int max_depth, int row, int col, int first_move, vector<int> &valid_moves
) {
    auto const& curr = tiles[row * 7 + col];

    if (curr.has_pearl()) return first_move;
    if (depth == max_depth) return -1;

    seen[row][col] = true;

    for (int i = 0; i < 4; i++) {
        if (depth == 0 && !valid_moves[i]) continue;
        auto direction = directions[i];

        pair<int, int> offset = direction.get_offset();
        int nr = row + offset.second;
        int nc = col + offset.first;

        // Inbounds of grid, not visited and no kelp blocking
        if ((nr < 0 || nr >= 7 || nc < 0 || nc >= 7) ||
            seen[nr][nc] || !curr.get_edge(direction).is_passable()
        ) {
            continue;
        }

        // Check that no dragon is in this tile
        if (tiles[nr * 7 + nc].get_dragon()) continue;

        int result = depth_dfs(tiles, depth+1, max_depth, nr, nc, depth == 0 ? i : first_move, valid_moves);
        if (result != -1) {
            seen[row][col] = false;
            return result;
        }
    }

    seen[row][col] = false;
    return -1;
}


// True if move is dangerous - DEFAULT + KING
bool danger_dfs(const vector<Tile> &tiles, int depth,
    int max_depth, int row, int col, char team
) {
    auto const& curr = tiles[row * 7 + col];

    // If its an enemy head
    if (tiles[row * 7 + col].get_dragon() && tiles[row * 7 + col].get_dragon()->is_dragon_head && tiles[row * 7 + col].get_dragon()->get_team().value != team) return true;

    // Nothing found in max range
    if (depth == max_depth) {
        return false;
    }

    seen[row][col] = true;

    for (int i = 0; i < 4; i++) {
        auto direction = directions[i];

        pair<int, int> offset = direction.get_offset();
        int nr = row + offset.second;
        int nc = col + offset.first;

        // Inbounds of grid, not visited and no kelp blocking
        if ((nr < 0 || nr >= 7 || nc < 0 || nc >= 7) ||
            seen[nr][nc] || !curr.get_edge(direction).is_passable()
        ) {
            continue;
        }

        // Check that no dragon body in this tile (we can't attack the body)
        if (tiles[nr * 7 + nc].get_dragon() && !tiles[nr * 7 + nc].get_dragon()->is_dragon_head && tiles[nr * 7 + nc].get_dragon()->get_team().value == team)  continue;

        if (danger_dfs(tiles, depth+1, max_depth, nr, nc, team)) {
            seen[row][col] = false;
            return true;
        }
    }

    seen[row][col] = false;
    return false;
}