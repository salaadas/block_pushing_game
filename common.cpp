#include "common.h"

Context global_context = {};
Temporary_Storage __default_temporary_storage = {};

void *heap_allocator(Allocator_Mode mode, i64 nbytes, i64 old_nbytes,
                     void *old_memory_pointer, void *allocator_data)
{
    // printf("Using heap allocator, mode %d, size %ld, data_ptr: %p\n",
    //        static_cast<i32>(mode), nbytes, old_memory_pointer);
    if (allocator_data != NULL) logprint("heap_allocator", "??? Allocating on the heap does not use 'allocator_data'...\n");

    if (mode == Allocator_Mode::ALLOCATE)
    {
        auto memory = malloc(nbytes);
        return memory;
    }
    else if (mode == Allocator_Mode::RESIZE)
    {
        return realloc(old_memory_pointer, nbytes);
    }
    else if (mode == Allocator_Mode::FREE)
    {
        free(old_memory_pointer);
        return NULL;
    }
    else if (mode == Allocator_Mode::FREE_ALL)
    {
        // Heap allocator does not have free all method
        free(old_memory_pointer);
        return NULL;
    }

    assert(0);
}

const Allocator_Proc __default_allocator = heap_allocator;

i64 get_temporary_storage_mark()
{
    return global_context.temporary_storage->occupied;
}

void reset_temporary_storage()
{
    set_temporary_storage_mark(0);
    global_context.temporary_storage->high_water_mark = 0;
}

void set_temporary_storage_mark(i64 mark)
{
    assert(mark >= 0);
    assert(mark <= global_context.temporary_storage->size);

    global_context.temporary_storage->occupied = mark;
}

void log_ts_usage()
{
    auto ts = global_context.temporary_storage;
    printf("in TS, occupied %ld, size %ld, water mark %ld, pointer %p \n",
           ts->occupied, ts->size, ts->high_water_mark, ts->data);
}

void *__temporary_allocator(Allocator_Mode mode,
                            i64   size,       i64   old_size,
                            void *old_memory, void *allocator_data)
{
    auto ts = static_cast<Temporary_Storage*>(allocator_data);

    if (mode == Allocator_Mode::ALLOCATE)
    {
        if (ts->data == NULL) // Therefore this is the first time calling
        {
            logprint("temporary_storage", "First time using TS, setting the storage of the TS....\n");
            ts->data = static_cast<u8*>(my_alloc(ts->size, {NULL, __default_allocator}));
            assert(ts->data != NULL);
        }

        if ((ts->occupied + size) > ts->size)
        {
            logprint("temporary_storage", "TS is too small to allocate an extra %ld bytes....\n", size);
            logprint("temporary_storage", "occupied: %ld, size: %ld, high_water_mark: %ld\n", ts->occupied, ts->size, ts->high_water_mark);
            assert(0);
        }

        void *memory_start = ts->data + ts->occupied;

        // @Todo: Aligns allocated temp memory to 8 bytes
        size = (size + 7) & ~7;

        ts->occupied += size;
        ts->high_water_mark = std::max(ts->high_water_mark, ts->occupied);

        return memory_start;
    }
    else if (mode == Allocator_Mode::RESIZE)
    {
        // Bump allocator does not resize stuff, instead
        // it allocates a new chunk of memory to be the same
        // as the new size
        ts->data = reinterpret_cast<u8*>(__temporary_allocator(Allocator_Mode::ALLOCATE, size, 0, 0, allocator_data));
        assert(ts->data != NULL);

        if (old_memory && (old_size > 0)) memcpy(ts->data, old_memory, std::min(old_size, size));

        return ts->data;
    }
    else if (mode == Allocator_Mode::FREE)
    {
        // Bump allocator does not free individual elements
        // The strategy is ``fire and forget'', which is
        // allocate all the stuff and freeing everything at the end
        logprint("temporary_storage", "[WARNING]: Attempting to free individual elements with TS.......\n");
        return NULL;
    }
    else if (mode == Allocator_Mode::FREE_ALL)
    {
        my_free(ts->data, {NULL, __default_allocator});
        ts->size            = 0;
        ts->occupied        = 0;
        ts->high_water_mark = 0;
        return NULL;
    }

    assert(0);
}

#include <stdarg.h>
void __logprint(const char *func, long line, const char *agent, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    printf("[%s]: ", agent);
    //printf("[%s] Function '%s', line %ld: ", agent, func, line);

    // Ignore the security warning....
    vprintf(fmt, args);

    va_end(args);
}

void *my_alloc(i64 size, Allocator allocator)
{
    auto a = allocator;
    if (!a) a = global_context.allocator;
    
    return a.proc(Allocator_Mode::ALLOCATE, size, 0, NULL, a.data);
}

void my_free(void *memory, Allocator allocator)
{
    auto a = allocator;
    if (!a) a = global_context.allocator;

    a.proc(Allocator_Mode::FREE, 0, 0, memory, a.data);
}

// Math stuff
f32 lerp(f32 x, f32 y, f32 t)
{
    return x * (1.f - t) + y * t;
}

Vector2 lerp(Vector2 x, Vector2 y, f32 t)
{
    return x * (1.f - t) + y * t;
}

Vector3 lerp(Vector3 x, Vector3 y, f32 t)
{
    return x * (1.f - t) + y * t;
}

Vector4 lerp(Vector4 x, Vector4 y, f32 t)
{
    return x * (1.f - t) + y * t;
}

Quaternion lerp(Quaternion a, Quaternion b, f32 t)
{
    Quaternion r;
    r.x = a.x + t * (b.x - a.x);
    r.y = a.y + t * (b.y - a.y);
    r.z = a.z + t * (b.z - a.z);
    r.w = a.w + t * (b.w - a.w);

    return r;
}

void normalize_or_identity(Quaternion *q)
{
    auto sq = sqrtf(q->x*q->x + q->y*q->y + q->z*q->z + q->w*q->w);
    if (sq == 0)
    {
        q->x = 0;
        q->y = 0;
        q->z = 0;
        q->w = 1;
        return;
    }

    auto factor = 1.0f / sq;
    q->x *= factor;
    q->y *= factor;
    q->z *= factor;
    q->w *= factor;
}

Quaternion nlerp(Quaternion a, Quaternion b, f32 t)
{
    auto r = lerp(a, b, t);
    normalize_or_identity(&r);
    return r;
}

Quaternion negate(Quaternion q)
{
    Quaternion r;
    r.x = -q.x;
    r.y = -q.y;
    r.z = -q.z;
    r.w = -q.w;

    return r;
}

Vector2 rotate(Vector2 v, f32 theta)
{
    auto ct = cosf(theta);
    auto st = sinf(theta);

    auto x = v.x*ct + v.y*-st;
    auto y = v.x*st + v.y* ct;

    v.x = x;
    v.y = y;

    return v;
}

Vector2 unit_vector(Vector2 v)
{
    auto sq = sqrtf(v.x*v.x + v.y*v.y);
    if (sq == 0) return v;

    Vector2 result;
    auto factor = 1.0f / sq;
    result.x = v.x * factor;
    result.y = v.y * factor;

    return result;
}

Vector3 unit_vector(Vector3 v)
{
    auto sq = sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
    if (sq == 0) return v;

    Vector3 result;
    auto factor = 1.0f / sq;
    result.x = v.x * factor;
    result.y = v.y * factor;
    result.z = v.z * factor;

    return result;
}

void print_cmaj_as_rmaj(Matrix4 mat)
{
    mat = glm::transpose(mat);

    printf("%f %f %f %f\n", mat[0][0], mat[0][1], mat[0][2], mat[0][3]);
    printf("%f %f %f %f\n", mat[1][0], mat[1][1], mat[1][2], mat[1][3]);
    printf("%f %f %f %f\n", mat[2][0], mat[2][1], mat[2][2], mat[2][3]);
    printf("%f %f %f %f\n", mat[3][0], mat[3][1], mat[3][2], mat[3][3]);
}

void set_rotation(Matrix4 *rotation_matrix, Quaternion orientation)
{
    *rotation_matrix = glm::toMat4(orientation);
}

void get_ori_from_rot(Quaternion *ori, Vector3 axis_of_rot, f32 theta)
{
    f32 sin_of_half_theta = sin(theta / 2.0f);
    ori->x = axis_of_rot.x * sin_of_half_theta;
    ori->y = axis_of_rot.y * sin_of_half_theta;
    ori->z = axis_of_rot.z * sin_of_half_theta;
    ori->w = cos(theta / 2.0f);
}

void get_rot_mat(Matrix4 *mat, Vector3 axis_of_rot, f32 theta)
{
    Matrix4 ident(1.0f);
    *mat = glm::rotate(ident, theta, axis_of_rot);
}

Vector3 rotate(Vector3 v, Quaternion ori)
{
    Vector3 qv = Vector3(ori.x, ori.y, ori.z);
    f32 s = ori.w;

    return
        static_cast<f32>(2.0*glm::dot(v, qv))*qv
        + (s*s - glm::dot(qv, qv))*v
        + 2.0f*s*glm::cross(qv, v);
}

f32 normalize_or_zero(Vector3 *dir)
{
    f32 length = glm::length(*dir);

    if (length == 0)
    {
        *dir = Vector3(0, 0, 0);
        return length;
    }

    f32 inversed_length = 1 / length;
    dir->x *= inversed_length;
    dir->y *= inversed_length;
    dir->z *= inversed_length;

    return length;
}

f32 sign_float(f32 x)
{
    return (0.0f < x) - (x < 0.0f);
}

f32 move_toward(f32 a, f32 b, f32 amount)
{
    if (a > b)
    {
        a -= amount;
        if (a < b) a = b;
    }
    else
    {        
        a += amount;
        if (a > b) a = b;
    }

    return a;
}

Vector3 move_toward(Vector3 a, Vector3 b, f32 amount)
{
    Vector3 result;
    result.x = move_toward(a.x, b.x, amount);
    result.y = move_toward(a.y, b.y, amount);
    result.z = move_toward(a.z, b.z, amount);

    return result;
}
