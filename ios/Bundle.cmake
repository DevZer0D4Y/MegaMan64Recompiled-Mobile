target_link_libraries(MegaMan64Recompiled PRIVATE SDL2::SDL2 SDL2::SDL2main
    "-framework UIKit" "-framework Foundation" "-framework Metal" "-framework QuartzCore"
    "-framework UniformTypeIdentifiers" "-framework GameController")
set_target_properties(MegaMan64Recompiled PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/ios/Info.plist"
    XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED NO)
add_custom_command(TARGET MegaMan64Recompiled POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/icons/icon.png" "$<TARGET_BUNDLE_DIR:MegaMan64Recompiled>/AppIcon.png"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_BUNDLE_DIR:MegaMan64Recompiled>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/recompcontrollerdb.txt" "$<TARGET_BUNDLE_DIR:MegaMan64Recompiled>/recompcontrollerdb.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/ios/icons" "$<TARGET_BUNDLE_DIR:MegaMan64Recompiled>"
    VERBATIM)
