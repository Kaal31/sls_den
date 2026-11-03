// Native PS5 publication admission is unsupported in the Windows port.
// Preserve the full parser on all platforms; never grant a lease from this host.
#include "pw_title_publication_native.h"
#include <string.h>
#include <errno.h>
static int deny(char *error, size_t capacity) {
    static const char message[] = "PS5 publication admission is unavailable on Windows";
    if (error && capacity) {
        size_t n = sizeof(message)-1;
        if (n >= capacity) n = capacity-1;
        memcpy(error, message, n);
        error[n] = 0;
    }
    errno = ENOSYS;
    return PW_TITLE_PUBLICATION_ADMISSION_INVALID;
}
int pw_title_publication_verify_locked(
    const PwTitlePublicationDescriptor *descriptor,
    const PwTitlePublicationPlatform *platform, int borrowed_lock_fd,
    const uint8_t *expected_receipt_bytes, size_t expected_receipt_length,
    const char expected_metadata_sha256[65], PwTitlePublicationContents *contents,
    char *error, size_t error_size) {
    (void)descriptor;(void)platform;(void)borrowed_lock_fd;
    (void)expected_receipt_bytes;(void)expected_receipt_length;
    (void)expected_metadata_sha256;(void)contents;
    return deny(error,error_size);
}
void pw_title_publication_lease_init(PwTitlePublicationLease *lease) {
    if (lease) { memset(lease,0,sizeof(*lease)); lease->lock_fd = -1; }
}
void pw_title_publication_close(PwTitlePublicationLease *lease) {
    // Windows never acquires a PS5 lease. Do not close arbitrary borrowed FDs.
    if (lease) pw_title_publication_lease_init(lease);
}
int pw_title_publication_descriptor_parse(
    const uint8_t *bytes, size_t length, const char *expected_title_id,
    PwTitlePublicationDescriptor *descriptor, char *error, size_t error_size) {
    (void)bytes;(void)length;(void)expected_title_id;(void)descriptor;
    return deny(error,error_size);
}
int pw_title_publication_read_descriptor(
    const char *canonical_path, const char *expected_title_id,
    const PwTitlePublicationPlatform *platform,
    PwTitlePublicationDescriptor *descriptor, char *error, size_t error_size) {
    (void)canonical_path;(void)expected_title_id;(void)platform;(void)descriptor;
    return deny(error,error_size);
}
int pw_title_publication_open(
    const PwTitlePublicationDescriptor *before,
    const PwTitlePublicationPlatform *platform,
    PwTitlePublicationLease *lease, char *error, size_t error_size) {
    (void)before;(void)platform;(void)lease;
    return deny(error,error_size);
}
