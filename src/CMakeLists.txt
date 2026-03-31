add_executable(shatv
    main.cpp
)

target_link_libraries(shatv
    PRIVATE
        Qt6::Core
        Qt6::Widgets
)
