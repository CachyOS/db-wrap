file(GLOB files example_*.cpp)

foreach(file ${files})
    get_filename_component(file_basename ${file} NAME_WE)
    add_executable(${file_basename} ${file})
    target_link_libraries(${file_basename} PRIVATE db-wrap::db-wrap libpqxx::pqxx Boost::pfr)
endforeach()
