add_plugin(MyReSTIRTracer)

target_sources(MyReSTIRTracer PRIVATE
    MyReSTIRTracer.cpp
    MyReSTIRTracer.h
    MyReSTIRTracer.rt.slang
)

target_copy_shaders(MyReSTIRTracer RenderPasses/MyReSTIRTracer)

target_source_group(MyReSTIRTracer "RenderPasses")
