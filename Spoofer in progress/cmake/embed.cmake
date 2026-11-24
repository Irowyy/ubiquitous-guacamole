if(NOT EXISTS "${IN}")
  message(FATAL_ERROR "entree introuvable : ${IN}")
endif()

file(READ "${IN}" hex HEX)
string(LENGTH "${hex}" hexlen)
if(hexlen GREATER 0)
  string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," hex "${hex}")
endif()

file(WRITE "${OUT}"
"// genere au build, ne pas editer
#include <cstddef>
namespace saryx {
extern const unsigned char ${SYM}[] = {${hex}};
extern const size_t ${SYM}Size = sizeof(${SYM});
}
")