FROM kalilinux/kali-rolling

WORKDIR /root

# Update and install necessary packages
RUN apt-get update && apt-get -y install wget git gcc-arm-none-eabi unzip sed make python3

# fetch nRF5 SDK 15.3.0
RUN wget https://www.nordicsemi.com/-/media/Software-and-other-downloads/SDKs/nRF5/Binaries/nRF5SDK153059ac345.zip && \
    unzip nRF5SDK153059ac345.zip && \
    rm nRF5SDK153059ac345.zip

# Download uf2conv.py and uf2families.json for AprBrother image conversion (Intel HEX to UF2)
ADD https://raw.githubusercontent.com/microsoft/uf2/master/utils/uf2conv.py /root/
ADD https://raw.githubusercontent.com/microsoft/uf2/master/utils/uf2families.json /root/

# patch SDK to use Kali's arm-none-eabi toolchain (SED's delimiter changed to allow inline path)
RUN sed -i "s#^GNU_INSTALL_ROOT.*#GNU_INSTALL_ROOT \?= /usr/bin/#g" nRF5_SDK_15.3.0_59ac345/components/toolchain/gcc/Makefile.posix

# Assuming LOGITacker directory structure is copied correctly into the image, no need to clone from GitHub
COPY . /root/LOGITacker

# Set up and build for MakerDiary MDK Dongle
WORKDIR /root/LOGITacker/mdk-dongle/blank/armgcc
RUN sed -i "s#^SDK_ROOT.*#SDK_ROOT := /root/nRF5_SDK_15.3.0_59ac345#g" Makefile && make


# Collect results into /root/build directory
WORKDIR /root
RUN mkdir build && \
    cp LOGITacker/mdk-dongle/blank/armgcc/_build/logitacker_mdk_dongle.hex build && \
    python3 uf2conv.py build/logitacker_mdk_dongle.hex -c -f 0xADA52840 -o build/logitacker_mdk_dongle.uf2
