#pragma once

/*
 * Búfer circular ligera (Lock-free Ring Buffer) de un productor y un
 * consumidor (SPSC).
 *
 *   - El productor (una ISR de CAN) hace una unica operacion: copiar la trama
 *     al buffer y avanzar el indice. Se libera la interrupcion en microsegundos.
 *   - El consumidor (el bucle principal) drena las tramas en tick().
 *
 * Reserva una casilla para distinguir lleno de vacio. Los indices son
 * volatiles y la memoria se ordena con una barrera de compilador. En AVR
 * (indices de 8/16 bits) las operaciones de indice son atomicas; en entornos
 * con RTOS el usuario puede inyectar lock/unlock en SystemApi si el
 * productor y el consumidor corren en nucleos distintos.
 */

#include <stddef.h>
#include <stdint.h>

namespace pcd {

template <typename T, size_t Capacity>
class RingBuffer {
  public:
    RingBuffer() : head_(0), tail_(0) {}

    bool push(const T &item) {
        const size_t next = (head_ + 1) % Capacity;
        if (next == tail_) {
            return false;  // lleno
        }
        buffer_[head_] = item;
        /* Barrera de compilador: el item queda escrito antes de que avance
         * el indice. En un CPU multinucleo real el usuario debe usar una
         * barrera de memoria de hardware (o lock()) en push/pop. */
        __asm__ __volatile__("" ::: "memory");
        head_ = next;
        return true;
    }

    bool pop(T &item) {
        if (tail_ == head_) {
            return false;  // vacio
        }
        item = buffer_[tail_];
        __asm__ __volatile__("" ::: "memory");
        tail_ = (tail_ + 1) % Capacity;
        return true;
    }

    size_t count() const { return (head_ - tail_ + Capacity) % Capacity; }
    bool empty() const { return tail_ == head_; }
    bool full() const { return ((head_ + 1) % Capacity) == tail_; }

    /* Descartar todos los elementos pendientes (por ejemplo tras bus-off). */
    void clear() {
        tail_ = head_;
    }

  private:
    volatile size_t head_;
    volatile size_t tail_;
    T buffer_[Capacity];
};

}  // namespace pcd
