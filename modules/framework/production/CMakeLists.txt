project(production)

file(GLOB SCRIPTS *.prototxt)

install(FILES ${SCRIPTS} DESTINATION params/framework/production/)
