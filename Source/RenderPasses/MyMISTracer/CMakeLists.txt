add_plugin(MyMISTracer)

target_sources(MyMISTracer PRIVATE
    MyMISTracer.cpp
    MyMISTracer.h
)

target_source_group(MyMISTracer "RenderPasses")
