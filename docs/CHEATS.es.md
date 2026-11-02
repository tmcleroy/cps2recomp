# New Cheats: guía completa

[English](CHEATS.md) | Español

Esta guía explica **todos los cheats añadidos o modificados por Input Lab**. Los cheats oficiales de
entrenamiento, invulnerabilidad, barras y otros conservan sus propias descripciones dentro del menú.
Los agregados aparecen en **Game → Cheats** y en la página de cada juego del launcher. Se guardan
**por juego**; los selectores P1/P2 son también independientes por jugador. Comienzan en Off/Vanilla.
Se mantienen las restricciones oficiales para cheats en partidas online.

| Cheat | Juego / conjunto base validado | Alcance |
|---|---|---|
| Damage hits | SFA `sfa` Europe 950727; SFA2 `sfa2u` USA 960430; SFZ2AL `sfz2al` Asia 960826; Super Turbo `ssf2xjr1` Japan 940223 | Igualación del perfil de daño humano/CPU actual |
| P1 / P2 Select Character | Super Turbo `ssf2xjr1` Japan 940223 | Personaje de la próxima inicialización, incluidos Akuma seleccionable y Shin Akuma jefe CPU |
| Select Speed | Super Turbo `ssf2xjr1` Japan 940223 | Regla original de turbo/frameskip interno |
| P1 / P2 Select Colour | Super Turbo `ssf2xjr1` Japan 940223 | Diez opciones comunes, con correspondencia correcta para las dos paletas de Akuma/Shin Akuma |
| Hidden characters | SFA2 `sfa2u` USA 960430 | Formas ocultas/clásicas mediante START |
| Hidden characters | Vampire Savior `vsavj` Japan 970519 | Dark Gallon/Oboro Bishamon con START y Shadow conservado |
| Hidden characters | Puzzle Fighter `spf2t` Europe 960529 | Ciclo de personajes ocultos con START |

Las direcciones y las instrucciones se comprobaron para estas revisiones base. Un nombre o una fecha
parecidos no garantizan compatibilidad con otra región. Los XML de MAMEPPK son un proyecto separado;
este fork implementa hooks nativos y no distribuye dichos XML.

## Damage hits

**Objetivo:** escoger qué perfil original de daño se usa cuando un humano lucha contra la CPU, sin
convertir al humano en un luchador controlado por la CPU ni reemplazar la fórmula por un multiplicador fijo.

| Opción | Efecto |
|---|---|
| **Vanilla** | Cálculo original y distinción humano/CPU original. |
| **Equal to human** | Ambos lados usan el perfil actual humano-atacando-CPU. La CPU utiliza también esa referencia en los cálculos correspondientes. |
| **Equal to CPU** | Ambos lados usan el perfil actual CPU-atacando-humano. El humano utiliza también esa referencia en los cálculos correspondientes. |

«Actual» significa que siguen operando la dificultad y los rangos adaptativos. Equal to CPU **no fuerza
el daño máximo**, y su perfil no tiene por qué ser mayor en toda dificultad o momento. Se iguala el
perfil pertinente, no el daño final de todos los personajes y movimientos. Se conservan los modificadores
de personaje, movimiento, bloqueo, vida, combo y azar. No se cambia la IA ni se sobrescriben los rangos almacenados.

**Ejemplo:** Equal to human permite comparar a la CPU con la referencia humana; Equal to CPU prueba el
sentido inverso. Un mismo movimiento puede seguir quitando distinta vida si sus circunstancias cambian.
Volver a Vanilla restaura la política original para el próximo cálculo, sin recrear al luchador.

**Rutas contempladas:**

- **SFA:** daño normal, agarres/throws y propiedad original del tercer luchador.
- **SFA2:** rutas normales/alternativas, throws y holds; conserva la implementación propia anterior.
- **SFZ2AL:** tercer luchador de Dramatic Battle y propiedad de proyectiles/ayudantes. Se ejecuta la rutina
  original sustituyendo solamente las lecturas pertinentes de rango/clase.
- **Super Turbo:** lecturas de rango de ataque/defensa de golpes, proyectiles y throws.

PvP normal y CPU contra CPU conservan Vanilla. Dramatic Battle requiere tratamiento específico por juego;
no implica que todos los modos alternativos de cualquier título hayan recibido esta política. No se añade
ni se afirma un supuesto bonus de daño por machacar botones.

Fuentes: [referencias comunes](../runtime/damage_policy.h), [SFA](../games/sfa/src/inputlab_damage.cpp),
[SFA2](../games/sfa2/src/damage.cpp), [SFZ2AL](../games/sfz2al/src/inputlab_damage.cpp),
[Super Turbo](../games/ssf2t/src/inputlab_cheats.cpp).

## Super Turbo: P1 / P2 Select Character

**Objetivo:** forzar el personaje al inicializarse la próxima selección de ese jugador. Cada lado ofrece
Off, los dieciséis personajes normales, **Akuma / Gouki** y **Shin Akuma / Gouki (CPU boss version)**.

Elegir **antes de confirmar personaje**. Cambiar durante una pelea afecta a la próxima inicialización.
Off deja de forzar futuras selecciones; no transforma retroactivamente al luchador activo durante una animación.

Akuma es la versión seleccionable normal. Shin Akuma conecta los comandos humanos con recursos originales
del jefe CPU: ataques, animaciones, vueltas del Tatsumaki, animación del teleport, doble proyectil aéreo
y trayectoria, además de la entrega original del hitstop del atacante al aceptar una cancelación de normal
a especial. El proyectil de suelo utiliza también la ruta del jefe.

**Límites:** no activa control CPU, no fuerza a Ken como rival, no iguala el daño, no agranda hitboxes,
no elimina separación ni modifica juggle. Damage hits es una opción aparte. Tamaño del rival, distancia,
timing y reglas originales siguen influyendo. Se comparó el puente con rutinas CPU originales, pero
**no se promete equivalencia 1:1 de todas las secuencias posibles**. Por ejemplo, proyectiles aéreos →
HP cercano → Shoryuken HP no tiene garantizados todos sus impactos contra cualquier personaje.

Fuentes: [hooks nativos](../games/ssf2t/src/inputlab_cheats.cpp) e
[inicialización de personajes](../games/ssf2t/src/choosing.cpp).

## Super Turbo: Select Speed

**Objetivo:** sustituir la comparación original del **frameskip interno del turbo**, independientemente
de overclock CPU, refresco de video, VSync o velocidad general de emulación.

| Opción | Efecto |
|---|---|
| **Off** | Recupera inmediatamente la elección original de la placa/jugador. |
| **No frameskip (like SSF2)** | Comparación sin descarte: se conserva cada paso del juego. |
| **Turbo 1–4 (Japan)** | Uno de los cuatro ajustes originales del turbo japonés. |
| **Maximum frameskip (double speed)** | Regla interna con el descarte más agresivo. |

Solo cambia el operando de la comparación original. No sobrescribe la velocidad guardada de placa/jugador
ni interpola imágenes nuevas. «Double speed» es el nombre del selector para esa regla, no una promesa
de duplicar una frecuencia medida en cualquier escena. Reducir el frameskip puede hacer más legible
una secuencia sin modificar sus reglas de impactos.

Fuente: SelectedInternalSpeed en [inputlab_cheats.cpp](../games/ssf2t/src/inputlab_cheats.cpp).

## Super Turbo: P1 / P2 Select Colour

**Objetivo:** forzar la paleta nativa asociada a una de las diez opciones normales. Es un selector;
no reasigna físicamente los botones de confirmación. Por ejemplo, elegir **LK button** fuerza esa paleta
aunque se confirme al personaje pulsando HP en el mando.

| Opción | Personajes normales | Akuma y Shin Akuma |
|---|---|---|
| **Off** | Elección original | Elección original |
| **LP / MP / HP button** | Su paleta individual correspondiente | Negro original, 0A |
| **LK / MK / HK button** | Su paleta individual correspondiente | Gris oscuro original, 0B |
| **Start button** | Paleta Start original | Gris oscuro, 0B |
| **Hold any P/K button** | Paleta original de botón mantenido | Gris oscuro, 0B |
| **SSF2 primary** | Paleta primaria del personaje original | Negro, 0A |
| **SSF2 alternate** | Paleta alternativa del personaje original | Gris oscuro, 0B |

Los dieciséis personajes normales conservan sus diez paletas. Akuma/Shin Akuma solo poseen dos conjuntos
nativos: las diez opciones se traducen a esos dos, sin consultar diez paletas inexistentes. No se inventan
colores ni se alteran gráficos, estadísticas o reglas de combate.

Elegir **antes de cargar los colores de la próxima ronda**. El cargador original copia el conjunto elegido
a VRAM. El retrato de Akuma sigue negro: el resultado se aprecia al entrar en combate. Off deja de forzar
el color; no reconstruye instantáneamente la paleta de una ronda ya iniciada.

Fuente: ApplyInputLabColour en [inputlab_cheats.cpp](../games/ssf2t/src/inputlab_cheats.cpp).

## Hidden characters: selección con START

Se modifican funciones ya existentes en la versión oficial. Se llaman **Hidden characters**, están en
**Cheats** y ofrecen **Off / Player 1 / Player 2 / Both**. Solo los lados habilitados reciben el acceso.
Cambian la selección, no las condiciones para que aparezca un retador CPU secreto.

### Street Fighter Alpha 2

Posicionarse sobre el personaje normal y pulsar/soltar **START** alterna su forma oculta. Pulsar START
otra vez vuelve a la normal; después se confirma de la manera habitual.

| Personaje señalado | Forma alternativa |
|---|---|
| Ryu | Evil Ryu |
| Akuma | Shin Akuma |
| Chun-Li | Chun-Li clásica |
| Dhalsim | Dhalsim clásico |
| Zangief | Zangief clásico |

Reemplaza la sustitución automática anterior: señalar a Ryu ya no fuerza Evil Ryu. La confirmación y los
colores disponibles siguen las reglas nativas; no se añaden paletas que una forma no posea. Tampoco fuerza
la aparición de Shin Akuma como retador de la escalera arcade.

### Vampire Savior

Sobre **Gallon**, START alterna **Dark Gallon**; sobre **Bishamon**, alterna **Oboro Bishamon**, la versión
de jefe final CPU que no se elige con un código normal de placa. Otro START vuelve al normal. Un puño
confirma los primeros colores; una patada, los segundos. El cuadro **?** conserva **Shadow**, que se
convierte en cada oponente vencido. Se aplica también a los luchadores de los modos alternativos compatibles.

### Super Puzzle Fighter II Turbo

En selección, START recorre **normal → Devilot → Akuma → Dan → normal**. Mover el cursor vuelve al personaje
normal señalado. Los dos botones de acción confirman sus respectivos colores nativos. Sustituye los
atajos directos mediante patadas, menos fáciles de descubrir; los códigos originales siguen funcionando
con el cheat apagado. Se conserva el comportamiento original de ataque de los ocultos, incluida su menor
potencia: no es un cheat para igualar ataque.

Las demás familias conservan sus implementaciones oficiales. Las fuentes compartidas distinguen SFA2
para que estas modificaciones no se impongan accidentalmente a Alpha 3 o SFZ2AL.

## Alcance de las comprobaciones

Las builds integradas 017/018 se comprobaron con rutinas originales y ejecución nativa: políticas de daño,
flags/ciclos/RAM, inicialización de jefe CPU frente al puente humano, cancelaciones, cargador original de
paletas y persistencia de menú. Con Select Colour apagado, 2100 imágenes coinciden con la 017.
Consultar [el registro del fork integrado](https://github.com/Sol1vaN/cps2recomp/blob/input-lab/docs/validation/inputlab-018.json). No certifica todas las regiones,
todos los combos imaginables ni un PR aislado que todavía no haya repetido las pruebas de integración.
