;
; Address of the static standard mouse driver
;
; Christian Groessler, 2014-01-02
;
; const void mouse_static_stddrv[];
;

        .export _mouse_static_stddrv
        .ifdef  __ATARIXL__
        .import _atrxjoy_mou
        .else
        .import _atrjoy_mou
        .endif

.rodata

        .ifdef  __ATARIXL__
_mouse_static_stddrv := _atrxjoy_mou
        .else
_mouse_static_stddrv := _atrjoy_mou
        .endif
