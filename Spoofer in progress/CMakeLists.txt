cmake_minimum_required(VERSION 3.21)
project(redmoon CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(NOT MSVC)
  message(FATAL_ERROR "MSVC (Visual Studio 2022) requis.")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "Cible x64 uniquement.")
endif()

set(PWF_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/pwf")
file(MAKE_DIRECTORY "${PWF_DIR}")
foreach(f IN ITEMS PwfClient.cpp PwfClient.h ServerPublicKey.h ProductionPolicy.h)
  if(NOT EXISTS "${PWF_DIR}/${f}")
    file(DOWNLOAD
      "https://raw.githubusercontent.com/pwfauth/pwfauth-cpp-client/v1.5.0/${f}"
      "${PWF_DIR}/${f}" STATUS st)
    list(GET st 0 code)
    if(NOT code EQUAL 0)
      file(DOWNLOAD
        "https://raw.githubusercontent.com/pwfauth/pwfauth-cpp-client/main/${f}"
        "${PWF_DIR}/${f}" STATUS st2)
    endif()
  endif()
endforeach()

include(FetchContent)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_Declare(nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(nlohmann_json)

function(redmoon_flags target)
  target_compile_options(${target} PRIVATE /utf-8 /W4 /permissive- /EHsc /MP)
  target_compile_definitions(${target} PRIVATE
    UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  target_include_directories(${target} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src" "${PWF_DIR}")
  target_link_libraries(${target} PRIVATE nlohmann_json::nlohmann_json)
endfunction()

function(redmoon_embed out symbol input)
  add_custom_command(OUTPUT "${out}"
    COMMAND ${CMAKE_COMMAND}
      -DIN=${input} -DOUT=${out} -DSYM=${symbol}
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed.cmake"
    DEPENDS "${input}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed.cmake"
    COMMENT "Embarquement de ${symbol}")
endfunction()

set(GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/gen")
file(MAKE_DIRECTORY "${GEN_DIR}")

set(PAYLOAD "${CMAKE_CURRENT_SOURCE_DIR}/assets/payload_astrax.dll")
if(EXISTS "${PAYLOAD}")
  redmoon_embed("${GEN_DIR}/blob_payload.cpp" kEmbeddedPayload "${PAYLOAD}")
else()
  message(WARNING "assets/payload_astrax.dll absent : redmoon.dll sera vide.")
  file(WRITE "${GEN_DIR}/blob_payload.cpp"
    "#include <cstddef>\nnamespace saryx {\nextern const unsigned char kEmbeddedPayload[] = {0};\nextern const size_t kEmbeddedPayloadSize = 0;\n}\n")
endif()

add_library(sideloaded SHARED
  src/modules/sideloaded_loader.cpp
  src/manualmap.cpp
  src/util.cpp)
set_target_properties(sideloaded PROPERTIES OUTPUT_NAME "sideloaded" PREFIX "")
redmoon_flags(sideloaded)
target_link_libraries(sideloaded PRIVATE advapi32 psapi shell32)

redmoon_embed("${GEN_DIR}/blob_loader.cpp" kEmbeddedLoader "$<TARGET_FILE:sideloaded>")

add_executable(redmoon
  src/main.cpp
  src/ui.cpp
  src/util.cpp
  src/evasion.cpp
  src/license.cpp
  src/loader.cpp
  src/pwf/PwfClient.cpp
  "${GEN_DIR}/blob_loader.cpp"
  "${GEN_DIR}/blob_payload.cpp")
redmoon_flags(redmoon)
target_link_libraries(redmoon PRIVATE
  advapi32 shell32 psapi bcrypt winhttp crypt32 ole32)
add_dependencies(redmoon sideloaded)

add_custom_command(TARGET redmoon POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "$<TARGET_FILE:sideloaded>" "$<TARGET_FILE_DIR:redmoon>/sideloaded.dll")