// These implementation files aren't needed with slimarchive shim
#ifndef SLIMARCHIVE
#include "libs/serialization/src/archive_exception.cpp"
#include "libs/serialization/src/basic_archive.cpp"
#include "libs/serialization/src/basic_iarchive.cpp"
#include "libs/serialization/src/basic_iserializer.cpp"
#include "libs/serialization/src/basic_oarchive.cpp"
#include "libs/serialization/src/basic_oserializer.cpp"
#include "libs/serialization/src/basic_serializer_map.cpp"
#include "libs/serialization/src/extended_type_info.cpp"
#include "libs/serialization/src/extended_type_info_typeid.cpp"
#include "libs/serialization/src/void_cast.cpp"
#ifdef _WIN32
#include "libs/serialization/src/codecvt_null.cpp"
#endif
#endif
