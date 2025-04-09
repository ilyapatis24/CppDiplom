cmake_minimum_required(VERSION 3.20)
project(Finder)
set(PostgreSQL_ADDITIONAL_VERSIONS "14")


# Путь к библиотекам Boost
set(Boost_INCLUDE_DIRS "c:/vcpkg/")  

# Пути к файлам в проекте Spider относительно Finder
set(SPIDER_SOURCE_DIR "${CMAKE_SOURCE_DIR}/../Spider")

# Файлы для добавления из Spyder в Finder
set(SPIDER_FILES
    ${SPIDER_SOURCE_DIR}/database.h
    ${SPIDER_SOURCE_DIR}/database.cpp
    ${SPIDER_SOURCE_DIR}/ParcerINI.h
    ${SPIDER_SOURCE_DIR}/ParcerINI.cpp
)

set(PROJECT_SOURCES
    finder.h
    finder.cpp
    main.cpp
)

find_package(Boost 1.86.0 COMPONENTS locale REQUIRED)
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
endif()


add_executable(Finder ${PROJECT_SOURCES} ${SPIDER_FILES})

# Включение директорий Boost
include_directories(${Boost_INCLUDE_DIRS})

# Связывание библиотеки Boost с проектом
target_link_libraries(Finder ${Boost_LIBRARIES})


add_subdirectory("D:/CppDiplom/libpqxx-7.10.0" libpqxx-build)
target_compile_features(Finder PRIVATE cxx_std_17) # Включим C++17
target_link_libraries(Finder pqxx)