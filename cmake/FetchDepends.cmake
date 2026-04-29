# deps
if(DB_WRAP_USE_EXTERNAL_LIBPQXX)
    message(STATUS "DBWRAP: using external libpqxx")
    find_package(libpqxx 8 REQUIRED)
else()
    message(STATUS "DBWRAP: fetching libpqxx")
    CPMAddPackage(
      NAME libpqxx
      GITHUB_REPOSITORY jtv/libpqxx
      GIT_TAG 8.0.1
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
      # 5034bf55fb6bd1efa7a14955bbb7171c2b3491fc wants to build tests with Boost::core
      GIT_TAG db9451143a70cbd30ce5a72f4aa294d73c17b7ab
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
       GIT_TAG v2.5.1
       EXCLUDE_FROM_ALL YES
    )
endif()

# C++26 <meta> reflection support (P2996)
set(DB_WRAP_HAS_STD_REFLECTION FALSE CACHE INTERNAL "")
if(DB_WRAP_USE_STD_REFLECTION)
    set(DB_WRAP_HAS_STD_REFLECTION TRUE CACHE INTERNAL "" FORCE)
endif()
