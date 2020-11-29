/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Implementation of Jenkins one-at-a-time hash function. These choices are
 * optimized for code size and portability, rather than raw speed. But speed
 * should still be quite good.
 **/

#ifndef ANDROID_JENKINS_HASH_H
#define ANDROID_JENKINS_HASH_H


/* The Jenkins hash of a sequence of 32 bit words A, B, C is:
 * Whiten(Mix(Mix(Mix(0, A), B), C)) */

typedef uint32_t hash_t;
template<typename TKey>
hash_t hash_type(const TKey& key);

/* Built-in hash code specializations.
 * Assumes pointers are 32bit. */
#define ANDROID_INT32_HASH(T) \
        template <> inline hash_t hash_type(const T& value) { return hash_t(value); }
#define ANDROID_INT64_HASH(T) \
        template <> inline hash_t hash_type(const T& value) { \
                return hash_t((value >> 32) ^ value); }
#define ANDROID_REINTERPRET_HASH(T, R) \
        template <> inline hash_t hash_type(const T& value) { \
                return hash_type(*reinterpret_cast<const R*>(&value)); }

ANDROID_INT32_HASH(bool)

ANDROID_INT32_HASH(int8_t)

ANDROID_INT32_HASH(uint8_t)

ANDROID_INT32_HASH(int16_t)

ANDROID_INT32_HASH(uint16_t)

ANDROID_INT32_HASH(int32_t)

ANDROID_INT32_HASH(uint32_t)

ANDROID_INT64_HASH(int64_t)

ANDROID_INT64_HASH(uint64_t)

ANDROID_REINTERPRET_HASH(float, uint32_t)

ANDROID_REINTERPRET_HASH(double, uint64_t)

template<typename T>
inline hash_t hash_type(T* const& value) {
  return hash_type(uintptr_t(value));
}

#ifdef __clang__
__attribute__((no_sanitize("integer")))
#endif
inline uint32_t
JenkinsHashMix(uint32_t hash, uint32_t data) {
  hash += data;
  hash += (hash << 10);
  hash ^= (hash >> 6);
  return hash;
}

uint32_t JenkinsHashWhiten(uint32_t hash);

/* Helpful utility functions for hashing data in 32 bit chunks */
uint32_t JenkinsHashMixBytes(uint32_t hash, const uint8_t* bytes, size_t size);

uint32_t JenkinsHashMixShorts(uint32_t hash,
                              const uint16_t* shorts,
                              size_t size);

#endif  // ANDROID_JENKINS_HASH_H
