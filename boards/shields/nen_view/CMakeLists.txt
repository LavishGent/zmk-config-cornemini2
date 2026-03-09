if(CONFIG_SHIELD_NEN_VIEW)
    target_sources(app PRIVATE
        widgets/killua_status.c
    )
    target_include_directories(app PRIVATE widgets/)
endif()
