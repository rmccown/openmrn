/** \copyright
 * Copyright (c) 2015, Stuart W Baker
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are  permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \file EEPROM.hxx
 * This implements a common EEPROM abstraction.
 *
 * @author Stuart W. Baker
 * @date 22 January 2015
 */

#include <cstdint>

#include "Devtab.hxx"

/** Common base class for all EEPROM access.
 */
class EEPROM : public Node
{
protected:
    /** Constructor.
     * @param name device name
     * @param file_size maximum file size that we can grow to.
     */
    EEPROM(const char *name, size_t file_size)
        : Node(name)
        , fileSize(file_size)
    {
    }

    /** Destructor.
     */
    ~EEPROM()
    {
    }

    /** Write to the EEPROM.  NOTE!!! This is not necessarily atomic across
     * byte boundaries in the case of power loss.  The user should take this
     * into account as it relates to data integrity of a whole block.
     * @ref index index within EEPROM address space to start write
     * @ref buf data to write
     * @ref len length in bytes of data to write
     */
    virtual void write(unsigned int index, const void *buf, size_t len) = 0;

    /** Read from the EEPROM.
     * @ref index index within EEPROM address space to start read
     * @ref buf location to post read data
     * @ref len length in bytes of data to read
     */
    virtual void read(unsigned int index, void *buf, size_t len) = 0;

private:
    size_t fileSize; /**< Maximum file size we can grow to */

    /** Open a device.
     * @param file new file reference to this device
     * @param path file or device name
     * @param flags open flags
     * @param mode open mode
     * @return 0 upon success, negative errno upon failure
     */
    int open(File* file, const char *path, int flags, int mode) OVERRIDE;

    /** Read from a file or device.
     * @param file file reference for this device
     * @param buf location to place read data
     * @param count number of bytes to read
     * @return number of bytes read upon success, -errno upon failure
     */
    ssize_t read(File *file, void *buf, size_t count) OVERRIDE;

    /** Write to a file or device.
     * @param file file reference for this device
     * @param buf location to find write data
     * @param count number of bytes to write
     * @return number of bytes written upon success, -errno upon failure
     */
    ssize_t write(File *file, const void *buf, size_t count) OVERRIDE;

    /** Seek method.
     * @param file file reference for this device
     * @param offset offset in bytes from whence directive
     * @param whence SEEK_SET if to set the file offset to an abosolute position,
     *               SEEK_CUR if to set the file offset from current position
     * @return current offest, or -1 with errno set upon error.
     */
    off_t lseek(File* file, off_t offset, int whence) OVERRIDE;

    void enable() OVERRIDE {} /**< function to enable device */
    void disable() OVERRIDE {}; /**< function to disable device */

    /** Discards all pending buffers. Called after disable(). */
    void flush_buffers() OVERRIDE {}

    /** Default constructor.
     */
    EEPROM();

    DISALLOW_COPY_AND_ASSIGN(EEPROM);
};
