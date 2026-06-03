# Script para manejar ROOT_CA que es multilínea
# NOTA: ROOT_CA causa problemas con escapes. Si necesitas cambiarlo,
# edita directamente secrets.cpp o usa el valor por defecto.
Import("env")
# ROOT_CA se omite aquí porque causa problemas de escape con defines
# Usa el valor por defecto en secrets.cpp o edítalo manualmente
print("[add_env_defines] ROOT_CA omitido (usa valor por defecto en secrets.cpp)")
