from building import *

cwd = GetCurrentDir()
src = ['ws2812.c']
libm = []

if GetDepend(['WS2812B_TIMING_TABLE']) or GetDepend(['WS2812B_TIMING_SEARCH']) or GetDepend(['WS2812B_CHECK_EXTENDED']):
    src += ['ws2812_timing.c']

if GetDepend(['WS2812B_GAMMA_COMPUTE']):
    src += ['gamma/ws2812_gamma_compute.c']
    libm = ['m']

if GetDepend(['WS2812B_USING_EFFECT']):
    src += ['effect/ws2812_effect.c']
    for symbol, name in [
        ('WS2812B_EFFECT_SOLID', 'solid'),
        ('WS2812B_EFFECT_BLINK', 'blink'),
        ('WS2812B_EFFECT_BREATHE', 'breathe'),
        ('WS2812B_EFFECT_GRADIENT', 'gradient'),
        ('WS2812B_EFFECT_GRADATION', 'gradation'),
        ('WS2812B_EFFECT_CHASE', 'chase'),
        ('WS2812B_EFFECT_SEQUENCE', 'sequence'),
    ]:
        if GetDepend([symbol]):
            src += ['effect/ws2812_effect_%s.c' % name]

    if GetDepend(['WS2812B_USING_MSH']):
        src += ['ws2812b_msh.c']

CPPPATH = [cwd, cwd + '/effect', cwd + '/gamma']
group = DefineGroup('ws2812b', src, depend=['PKG_USING_WS2812B'], CPPPATH=CPPPATH, LIBS=libm)
Return('group')
