#include <xinu.h>  // Esto es como un manual de instrucciones para usar Xinu, un sistema operativo especial. Lo necesitamos para que el programa sepa cómo trabajar.

int32 seq = -1;  // Aquí creamos una caja especial llamada "seq" donde guardaremos un número entero (int32 significa un número grande). Empezamos con -1 como valor inicial, como si la caja estuviera vacía al principio.
sid32 mutex;     // Esto es como un candado que usaremos para que solo una persona use la caja "seq" a la vez.
sid32 empty;     // Un contador que dice cuántos espacios libres hay para poner números en la caja (empezará con 1).
sid32 full;      // Un contador que dice cuántos números listos hay para tomar de la caja (empezará con 0).

void producer(void) {  // Esta es la función del "productor", como un cocinero que hace números y los pone en la caja.
    int32 val = 0;     // Aquí el cocinero empieza con el número 0.
    while (val <= 800) {  // Mientras el número sea 800 o menos, sigue trabajando.
        wait(empty);     // El cocinero espera a que haya un espacio libre (si no, se queda quieto).
        wait(mutex);     // Pone el candado para que nadie más toque la caja mientras él trabaja.
        seq = val;       // Pone su número en la caja "seq".
        signal(mutex);   // Quita el candado para que otros puedan usar la caja.
        signal(full);    // Dice que hay un número listo para que alguien lo tome.
        val += 2;        // Añade 2 al número (así hace solo números pares: 0, 2, 4, ...).
    }
    signal(empty);     // Al final, deja un espacio libre extra para que el consumidor termine bien.
}

void consumer(void) {  // Esta es la función del "consumidor", como un cliente que toma números de la caja.
    int32 expected = 0;  // El cliente espera que el primer número sea 0.
    while (expected <= 800) {  // Mientras el número esperado sea 800 o menos, sigue revisando.
        wait(full);      // El cliente espera a que haya un número listo (si no, se queda quieto).
        wait(mutex);     // Pone el candado para mirar la caja sin que la cambien.
        if (seq != expected) {  // Si el número en la caja no es el que esperaba...
            printf("Error: expected %d, got %d\n", expected, seq);  // Se queja diciendo qué esperaba y qué encontró.
            signal(mutex);   // Quita el candado.
            signal(empty);   // Deja un espacio libre.
            return;          // Se va y termina porque algo salió mal.
        }
        signal(mutex);   // Quita el candado si todo está bien.
        signal(empty);   // Deja un espacio libre para el próximo número.
        expected += 2;   // Aumenta lo que espera (busca 2, 4, 6, ...).
    }
    printf("Secuencia OK\n");  // Si llega aquí, significa que todo salió perfecto.
}

int main(void) {  // Esta es la función principal que empieza todo el juego.
    mutex = semcreate(1);  // Crea el candado inicial, listo para uno solo (1).
    empty = semcreate(1);  // Crea el contador de espacios libres, empieza con 1.
    full = semcreate(0);   // Crea el contador de números listos, empieza con 0.
    resume(create(producer, 1024, 10, "prod", 0));  // Empieza al cocinero con prioridad 10 y un espacio de memoria.
    resume(create(consumer, 1024, 10, "cons", 0));  // Empieza al cliente con prioridad 10 y un espacio de memoria.
    sleep(1000);           // Espera 1000 "ticks" (como 1 segundo) para dar tiempo a que trabajen.
    printf("seq final: %d\n", seq);  // Muestra el número final que quedó en la caja.
    return OK;             // Termina el programa diciendo que todo salió bien (OK es un código especial).
}
