set(COMPONENT_SRCS "main.c" "blink.c" "ui.c")
set(COMPONENT_ADD_INCLUDEDIRS "." "./includes")

register_component()

target_add_binary_data(${COMPONENT_TARGET} "certs/aws-root-ca.pem" TEXT)
