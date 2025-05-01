#ifndef REGISTER_TYPES_H
#define REGISTER_TYPES_H

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;

void initialize_odin_module(ModuleInitializationLevel p_level);
void uninitialize_odin_module(ModuleInitializationLevel p_level);

#endif // REGISTER_TYPES_H

