IF (NOT QDB_VERSION)
    cmake_minimum_required (VERSION 2.8)

# not included from the marvellous qdb
    project (leveldb)    
    SET(CMAKE_DEBUG_POSTFIX "d")
    SET(QDB_DEBUG_POSTFIX "d")
    SET(QDB_RELEASE_POSTFIX "")

    IF(CMAKE_CL_64)
        SET(QDB_CPU_ARCH "64bit")
    ELSE(CMAKE_CL_64)
        IF (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "amd64" OR ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
            SET(QDB_CPU_ARCH "64bit")
        ELSE(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "amd64" OR ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
            SET(QDB_CPU_ARCH "32bit")
        ENDIF (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "amd64" OR ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
    ENDIF(CMAKE_CL_64)

# for Windows we need to add some information to find the "right" boost library dir
    IF (MSVC)
        IF(QDB_CPU_ARCH STREQUAL "64bit")
            MESSAGE(STATUS "Looking for 64-bit boost libraries")
            FILE(TO_CMAKE_PATH $ENV{BOOST_ROOT}/stage64/lib BOOST_LIBRARYDIR)
        ELSE(QDB_CPU_ARCH STREQUAL "64bit")
            MESSAGE(STATUS "Looking for 32-bit boost libraries")
            FILE(TO_CMAKE_PATH $ENV{BOOST_ROOT}/stage/lib BOOST_LIBRARYDIR)
        ENDIF(QDB_CPU_ARCH STREQUAL "64bit")
    ENDIF(MSVC)

    set(Boost_USE_STATIC_LIBS ON)
    set(Boost_USE_MULTITHREAD ON)
    set(Boost_USE_STATIC_RUNTIME OFF)

    set(Boost_ADDITIONAL_VERSIONS)

    find_package(Boost COMPONENTS 
        date_time
        filesystem
        system
        REQUIRED)

    SET(SNAPPY_LIBRARY "")

    STRING(REGEX MATCH "clang" CLANG ${CMAKE_CXX_COMPILER})

    IF(CMAKE_COMPILER_IS_GNUCXX OR CLANG)
        find_library(Pthread_LIBRARY pthread)
        find_library(Realtime_LIBRARY rt)
        # find library can be problematic with stdc++ which is why we hardwire the link
        SET(Stdcpp_LIBRARY stdc++)
    ELSE(CMAKE_COMPILER_IS_GNUCXX OR CLANG)
        SET(Pthread_LIBRARY "")
        SET(Realtime_LIBRARY "")
        SET(Stdcpp_LIBRARY "")
    ENDIF(CMAKE_COMPILER_IS_GNUCXX OR CLANG)

ELSE(NOT QDB_VERSION)
    SET(SNAPPY_LIBRARY "snappy")
ENDIF(NOT QDB_VERSION)

SET(LEVELDB_IDE_FOLDER "leveldb")

include_directories(${Boost_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${SNAPPY_INCLUDE_DIRS})

# /wd4722: constructor never returns, potential memory leak because of a sigleton pattern
# /wd4702: unreachable code because of singleton pattern
# /wd4800: bool cast performance warning
IF(MSVC)
    SET(LEVELDB_COMPILE_FLAGS "/D_CRT_SECURE_NO_WARNINGS /wd4800 /wd4722 /wd4702 /DLEVELDB_ATOMIC_PRESENT")
ELSE(MSVC)
    SET(LEVELDB_COMPILE_FLAGS "-Wno-sign-compare -DLEVELDB_ATOMIC_PRESENT")
ENDIF(MSVC)

SET(LEVEL_DB_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/include/leveldb/c.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/leveldb/cache.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/leveldb/comparator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/leveldb/db.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/leveldb/dumpfile.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/leveldb/env.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/leveldb/iterator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/leveldb/filter_policy.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/leveldb/iterator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/leveldb/options.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/leveldb/slice.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/leveldb/status.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/leveldb/table.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/leveldb/table_builder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/leveldb/write_batch.h
	${CMAKE_CURRENT_SOURCE_DIR}/db/builder.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/db/builder.h
	${CMAKE_CURRENT_SOURCE_DIR}/db/db_impl.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/db/db_impl.h
	${CMAKE_CURRENT_SOURCE_DIR}/db/db_iter.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/db/db_iter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/db/dbformat.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/db/dbformat.h
    ${CMAKE_CURRENT_SOURCE_DIR}/db/dumpfile.cc
	${CMAKE_CURRENT_SOURCE_DIR}/db/filename.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/db/filename.h
    ${CMAKE_CURRENT_SOURCE_DIR}/db/log_format.h
	${CMAKE_CURRENT_SOURCE_DIR}/db/log_reader.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/db/log_reader.h
	${CMAKE_CURRENT_SOURCE_DIR}/db/log_writer.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/db/log_writer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/db/skiplist.h
    ${CMAKE_CURRENT_SOURCE_DIR}/db/snapshot.h
	${CMAKE_CURRENT_SOURCE_DIR}/db/memtable.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/db/memtable.h
	${CMAKE_CURRENT_SOURCE_DIR}/db/repair.cc
	${CMAKE_CURRENT_SOURCE_DIR}/db/table_cache.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/db/table_cache.h
	${CMAKE_CURRENT_SOURCE_DIR}/db/version_edit.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/db/version_edit.h
	${CMAKE_CURRENT_SOURCE_DIR}/db/version_set.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/db/version_set.h
	${CMAKE_CURRENT_SOURCE_DIR}/db/write_batch.cc
	${CMAKE_CURRENT_SOURCE_DIR}/table/block.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/table/block.h
	${CMAKE_CURRENT_SOURCE_DIR}/table/block_builder.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/table/block_builder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/table/filter_block.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/table/filter_block.h
	${CMAKE_CURRENT_SOURCE_DIR}/table/format.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/table/format.h
	${CMAKE_CURRENT_SOURCE_DIR}/table/iterator.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/table/iterator_wrapper.h
	${CMAKE_CURRENT_SOURCE_DIR}/table/merger.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/table/merger.h
	${CMAKE_CURRENT_SOURCE_DIR}/table/table.cc
	${CMAKE_CURRENT_SOURCE_DIR}/table/table_builder.cc
	${CMAKE_CURRENT_SOURCE_DIR}/table/two_level_iterator.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/table/two_level_iterator.h
	${CMAKE_CURRENT_SOURCE_DIR}/util/arena.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/util/arena.h
    ${CMAKE_CURRENT_SOURCE_DIR}/util/bloom.cc
	${CMAKE_CURRENT_SOURCE_DIR}/util/cache.cc
	${CMAKE_CURRENT_SOURCE_DIR}/util/coding.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/util/coding.h
	${CMAKE_CURRENT_SOURCE_DIR}/util/comparator.cc
	${CMAKE_CURRENT_SOURCE_DIR}/util/crc32c.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/util/crc32c.h
	${CMAKE_CURRENT_SOURCE_DIR}/util/env.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/util/filter_policy.cc
	${CMAKE_CURRENT_SOURCE_DIR}/util/hash.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/util/hash.h
	${CMAKE_CURRENT_SOURCE_DIR}/util/histogram.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/util/histogram.h
	${CMAKE_CURRENT_SOURCE_DIR}/util/logging.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/util/logging.h
    ${CMAKE_CURRENT_SOURCE_DIR}/util/mutexlock.h
	${CMAKE_CURRENT_SOURCE_DIR}/util/options.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/util/random.h
	${CMAKE_CURRENT_SOURCE_DIR}/util/status.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/port/port.h)

IF(WIN32)
    SET(LEVEL_DB_FILES ${LEVEL_DB_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/port/port_win.h)
    SET(LEVEL_DB_FILES ${LEVEL_DB_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/port/port_win.cc)
    SET(LEVEL_DB_FILES ${LEVEL_DB_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/util/win_logger.h)
    SET(LEVEL_DB_FILES ${LEVEL_DB_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/util/win_logger.cc)
    SET(LEVEL_DB_FILES ${LEVEL_DB_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/util/env_boost.cc)
ELSE(WIN32)
    SET(LEVEL_DB_FILES ${LEVEL_DB_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/port/port_posix.h)
    SET(LEVEL_DB_FILES ${LEVEL_DB_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/port/port_posix.cc)
    SET(LEVEL_DB_FILES ${LEVEL_DB_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/util/posix_logger.h)    
    SET(LEVEL_DB_FILES ${LEVEL_DB_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/util/env_posix.cc)
ENDIF(WIN32)

# forward files list and include dirs to parent scope
IF(QDB_VERSION)
    SET(LEVEL_DB_SOURCE_FILES ${LEVEL_DB_FILES} PARENT_SCOPE)
    SET(LEVEL_DB_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/include PARENT_SCOPE)
ENDIF(QDB_VERSION)

add_library(leveldb ${LEVEL_DB_FILES})

set_target_properties(leveldb PROPERTIES 
    FOLDER ${LEVELDB_IDE_FOLDER} 
    COMPILE_FLAGS ${LEVELDB_COMPILE_FLAGS})

add_executable(leveldbutil
    ${CMAKE_CURRENT_SOURCE_DIR}/db/leveldb_main.cc)

target_link_libraries(leveldbutil
    leveldb
    ${SNAPPY_LIBRARY}
    ${Boost_LIBRARIES}
    ${Pthread_LIBRARY}
    ${Additional_LIBRARIES})

set_target_properties(leveldbutil PROPERTIES
    FOLDER ${LEVELDB_IDE_FOLDER}
    DEBUG_POSTFIX ${QDB_DEBUG_POSTFIX}
    COMPILE_FLAGS ${LEVELDB_COMPILE_FLAGS})

# we distribute the leveldbutil as it might be useful
install(TARGETS leveldbutil
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib)

##################################### TESTS #######################################
# Every leveldb test file has to be compiled as an independant binary
# because of the test framework used by leveldb.
SET(LEVELDB_TEST_RUNTIME
    ${CMAKE_CURRENT_SOURCE_DIR}/util/testutil.h
    ${CMAKE_CURRENT_SOURCE_DIR}/util/testutil.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/util/testharness.h
    ${CMAKE_CURRENT_SOURCE_DIR}/util/testharness.cc)

add_library(leveldb_test_rt ${LEVELDB_TEST_RUNTIME})

set_target_properties(leveldb_test_rt PROPERTIES
    FOLDER ${LEVELDB_IDE_FOLDER}
    COMPILE_FLAGS ${LEVELDB_COMPILE_FLAGS})

FUNCTION(LEVELDB_ADD_TEST TESTNAME TESTFILE)
    IF (NOT TESTNAME)
        MESSAGE(SEND_ERROR "Error: LEVELDB_ADD_TEST called without test name")
        RETURN()
    ENDIF(NOT TESTNAME)

    IF (NOT TESTFILE)
        MESSAGE(SEND_ERROR "Error: LEVELDB_ADD_TEST called without test file")
        RETURN()
    ENDIF(NOT TESTFILE)

    add_executable(leveldb_${TESTNAME}_test
        ${TESTFILE})

    target_link_libraries(leveldb_${TESTNAME}_test
        leveldb_test_rt

        leveldb
        ${SNAPPY_LIBRARY}
        ${Boost_LIBRARIES}
        ${Pthread_LIBRARY}
        ${Additional_LIBRARIES})

        set_target_properties(leveldb_${TESTNAME}_test PROPERTIES
            FOLDER ${LEVELDB_IDE_FOLDER}
            DEBUG_POSTFIX ${QDB_DEBUG_POSTFIX}
            COMPILE_FLAGS ${LEVELDB_COMPILE_FLAGS})

ENDFUNCTION(LEVELDB_ADD_TEST)

LEVELDB_ADD_TEST(env ${CMAKE_CURRENT_SOURCE_DIR}/util/env_test.cc)
LEVELDB_ADD_TEST(crc32 ${CMAKE_CURRENT_SOURCE_DIR}/util/crc32c_test.cc)
LEVELDB_ADD_TEST(coding ${CMAKE_CURRENT_SOURCE_DIR}/util/coding_test.cc)
LEVELDB_ADD_TEST(arena ${CMAKE_CURRENT_SOURCE_DIR}/util/arena_test.cc)
LEVELDB_ADD_TEST(cache ${CMAKE_CURRENT_SOURCE_DIR}/util/cache_test.cc)
LEVELDB_ADD_TEST(table ${CMAKE_CURRENT_SOURCE_DIR}/table/table_test.cc)
LEVELDB_ADD_TEST(autocompact ${CMAKE_CURRENT_SOURCE_DIR}/db/autocompact_test.cc)
LEVELDB_ADD_TEST(corruption ${CMAKE_CURRENT_SOURCE_DIR}/db/corruption_test.cc)
LEVELDB_ADD_TEST(dbformat ${CMAKE_CURRENT_SOURCE_DIR}/db/dbformat_test.cc)
LEVELDB_ADD_TEST(db ${CMAKE_CURRENT_SOURCE_DIR}/db/db_test.cc)
LEVELDB_ADD_TEST(filename ${CMAKE_CURRENT_SOURCE_DIR}/db/filename_test.cc)
LEVELDB_ADD_TEST(log ${CMAKE_CURRENT_SOURCE_DIR}/db/log_test.cc)
LEVELDB_ADD_TEST(skiplist ${CMAKE_CURRENT_SOURCE_DIR}/db/skiplist_test.cc)
LEVELDB_ADD_TEST(version_edit ${CMAKE_CURRENT_SOURCE_DIR}/db/version_edit_test.cc)
LEVELDB_ADD_TEST(write_batch ${CMAKE_CURRENT_SOURCE_DIR}/db/write_batch_test.cc)
LEVELDB_ADD_TEST(db_bench ${CMAKE_CURRENT_SOURCE_DIR}/db/db_bench.cc)
LEVELDB_ADD_TEST(version_set ${CMAKE_CURRENT_SOURCE_DIR}/db/version_set_test.cc)
LEVELDB_ADD_TEST(filter_block ${CMAKE_CURRENT_SOURCE_DIR}/table/filter_block_test.cc)
LEVELDB_ADD_TEST(bloom ${CMAKE_CURRENT_SOURCE_DIR}/util/bloom_test.cc)
LEVELDB_ADD_TEST(hash ${CMAKE_CURRENT_SOURCE_DIR}/util/hash_test.cc)