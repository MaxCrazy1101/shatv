add_test(
    NAME shatv_smoke
    COMMAND $<TARGET_FILE:shatv>
)

set_tests_properties(
    shatv_smoke
    PROPERTIES PASS_REGULAR_EXPRESSION "ShaTV bootstrap"
)
