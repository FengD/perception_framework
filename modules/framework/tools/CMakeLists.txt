project(framework_tools)
file(GLOB TOOL_FILES *.py)
install(FILES ${TOOL_FILES} DESTINATION python/)
