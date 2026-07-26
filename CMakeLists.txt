cmake_minimum_required(VERSION 3.16)
project(MoonTail C)

set(CMAKE_C_STANDARD 11)

add_executable(moontail client/moontail.c)
target_include_directories(moontail PRIVATE client)

add_executable(moontail-volunteer server/volunteer.c)
target_include_directories(moontail-volunteer PRIVATE client)

if(WIN32)
  target_compile_definitions(moontail PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_compile_definitions(moontail-volunteer PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
