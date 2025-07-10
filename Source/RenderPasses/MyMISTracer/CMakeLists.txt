add_plugin(MyMISTracer)

target_sources(MyMISTracer PRIVATE
    MyMISTracer.cpp
    MyMISTracer.h
    MyMISTracer.rt.slang
)

target_copy_shaders(MyMISTracer RenderPasses/MyMISTracer)

target_source_group(MyMISTracer "RenderPasses")
