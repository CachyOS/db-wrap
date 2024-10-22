# deps
if(DB_WRAP_USE_EXTERNAL_LIBPQXX)
    message(STATUS "DBWRAP: using external libpqxx")
    find_package(libpqxx 7.9 REQUIRED)
else()
    message(STATUS "DBWRAP: fetching libpqxx")
    CPMAddPackage(
      NAME libpqxx
      GITHUB_REPOSITORY jtv/libpqxx
      GIT_TAG 7.9.2
      EXCLUDE_FROM_ALL YES
    )
endif()
if(DB_WRAP_USE_EXTERNAL_PFR)
    message(STATUS "DBWRAP: using external pfr")
    find_package(boost_pfr REQUIRED)
else()
    message(STATUS "DBWRAP: fetching pfr")
    CPMAddPackage(
      NAME pfr
      GITHUB_REPOSITORY boostorg/pfr
      GIT_TAG 8cd1a9675d327a058101ef0802789f1830ac2a7e
      EXCLUDE_FROM_ALL YES
    )
endif()
if(DB_WRAP_USE_EXTERNAL_DOCTEST)
    message(STATUS "DBWRAP: using external doctest")
    find_package(doctest 2 REQUIRED)
else()
    message(STATUS "DBWRAP: fetching doctest")
    CPMAddPackage(
       NAME doctest
       GITHUB_REPOSITORY doctest/doctest
       GIT_TAG v2.4.11
       EXCLUDE_FROM_ALL YES
    )
endif()