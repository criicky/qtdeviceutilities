add_subdirectory(networksettings)
add_subdirectory(doc)

if(ANDROID)
    message("AMMAZZO QUALCUNO")
endif()

if(TARGET Qt::Qml)
    add_subdirectory(networksettingsqml)
endif()
