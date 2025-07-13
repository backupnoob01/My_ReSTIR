add_plugin(ReservoirsReuse)

target_sources(ReservoirsReuse PRIVATE
    ReservoirsReuse.cpp
    ReservoirsReuse.h
    ReservoirsReuse.cs.slang
)

target_copy_shaders(ReservoirsReuse RenderPasses/ReservoirsReuse)

target_source_group(ReservoirsReuse "RenderPasses")
