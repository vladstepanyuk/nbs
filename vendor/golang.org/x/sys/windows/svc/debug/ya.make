GO_LIBRARY()

LICENSE(BSD-3-Clause)

IF (OS_WINDOWS)
    SRCS(
        log.go
        service.go
    )
ENDIF()

END()
