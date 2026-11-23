#pragma once
#include <cstdint>
namespace ms {
inline bool training_buff(int32_t id) {
    switch(id) { case 5001005: case 1001003: case 1101004: case 1101005: case 1101006: case 1101007: case 1201004: case 1201005: case 1301004: case 1301005: case 1301006: case 1301007: case 2001002: case 2001003: case 2101001: case 2111005: case 2201001: case 2211005: case 2301003: case 2301004: case 2311003: case 3001003: case 3101002: case 3201002: case 4001003: case 4101003: case 4201002: case 5101006: case 5201003: return true; default: return false; }
}
inline bool training_skill(int32_t id) {
    switch(id) { case 0: case 1000: case 1001004: case 1001005: case 1111003: case 1111004: case 1111005: case 1121006: case 1121008: case 1211002: case 1221007: case 1221009: case 1221011: case 1311001: case 1311002: case 1311003: case 1311004: case 1311005: case 1311006: case 1321003: case 2001004: case 2001005: case 2101004: case 2101005: case 2111002: case 2111006: case 2121003: case 2121006: case 2121007: case 3001004: case 3001005: case 4001334: case 4001344: case 5001001: case 5001002: case 5001003: case 5101002: case 5201001: return true; default: return training_buff(id); }
}
}
