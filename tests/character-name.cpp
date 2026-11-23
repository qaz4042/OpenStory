#include "../src/Character/CharacterName.h"
#include <cassert>
int main() {
    using ms::valid_character_name_format;
    for (auto name : {"abc", "Kaka312", "ABC123xyz789", "123"})
        assert(valid_character_name_format(name));
    for (auto name : {"", "ab", "ABCDEFGHIJKLM", "卡卡寂312", "abc def", "abc_123", "abc!", "ＡＢＣ", "abc\n"})
        assert(!valid_character_name_format(name));
}
