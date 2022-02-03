#!/bin/env bash

# Directory to place Private Key and Cert files. (default: current working directory)
OUTPUT_DIR=$(pwd)

# Whether or not to use a config file when generating a certificate
USE_CONFIG=0
# The config file to be used, ignored if USE_CONFIG is 0
CONFIG_FILE=""
# Name of the certificate and key to generate
CERT_NAME="root"
# Whether or not to change the file permissions to 600 (off by default)
PROTECT_CERT=0
# Whether or not to run a test on the generated key
RUN_TEST=0

help () {
    echo "Usage: $0 [OPTIONS]..."
    echo "Generates a Local Root CA Private Key and Certificate File."
    echo "By default theses files are placed in the current working directory."
    echo "But you can optionally pass a directory to to place the files."
    echo
    echo "  -o, --output         directory to place private key and cert files."
    echo "                         (defaults to current directory)"
    echo "  -c, --config         a config file to pass to openssl for further"
    echo "                         configuration of the certificate."
    echo "                         (no custom configuration by default)"
    echo "  -n, --name           name of the private key and certificate."
    echo "                         (defaults to root.pem and root.crt)"
    echo "  -p, --protect        sets the permissions of the generated files to"
    echo "                         600 to protect them from being read by other"
    echo "                         users on the system."
    echo "  -t, --test           runs a test on the generated certificate to"
    echo "                         make sure it's ready for signing."
    echo "  -h, --help           displays this message and quits."
    echo
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        -h|--help) help; exit;;
        -o|--output) OUTPUT_DIR="$2"; shift 2;;
        -c|--config) USE_CONFIG=1; CONFIG_FILE="$2"; shift 2;;
        -n|--name) CERT_NAME="$2"; shift 2;;
        -p|--protect) PROTECT_CERT=1; shift 1;;
        -t|--test) RUN_TEST=1; shift 1;;

        -o=*|--output=*) OUTPUT_DIR="${1#*=}"; shift 1;;
        -c=*|--config=*) USE_CONFIG=1; CONFIG_FILE="${1#*=}"; shift 1;;
        -n=*|--name=*) CERT_NAME="${1#*=}"; shift 1;;

        -*) echo "Unknown option: $1" >&2; help; exit 1;;
        *) echo "Ignoring extra argument: $1"; shift 1;;
    esac
done

if [ ! -d $OUTPUT_DIR ]; then
    mkdir -p $OUTPUT_DIR
fi

echo Generating a private key for the CA...
if ! openssl genpkey -algorithm RSA -out "$OUTPUT_DIR/$CERT_NAME.pem" -aes-256-cbc; then
    echo Failed to generate a private key!
    exit 1
fi

echo Using generated key to make a certificate...
if [ $USE_CONFIG -eq 1 ] && [ -f $CONFIG_FILE ]; then
    if ! openssl req -new -x509 -config "$CONFIG_FILE" -key "$OUTPUT_DIR/$CERT_NAME.pem" -out "$OUTPUT_DIR/$CERT_NAME.crt"
    then
        echo Failed to generate a certificate!
        exit 1
    fi

else
    if ! openssl req -new -x509 -key "$OUTPUT_DIR/$CERT_NAME.pem" -out "$OUTPUT_DIR/$CERT_NAME.crt"
    then
        echo Failed to generate a certificate!
        exit 1
    fi
fi

if [ $PROTECT_CERT -eq 1 ]; then
    chmod 600 "$OUTPUT_DIR/$CERT_NAME.pem"
    chmod 600 "$OUTPUT_DIR/$CERT_NAME.crt"
fi

echo $OUTPUT_DIR/$CERT_NAME.pem and $OUTPUT_DIR/$CERT_NAME.crt successfully generated!

if [ $RUN_TEST -eq 1 ]; then
    echo Running tests on $OUTPUT_DIR/$CERT_NAME.crt
    if ! openssl x509 -noout -text -in "$OUTPUT_DIR/$CERT_NAME.crt"; then
        echo Test failed!
        exit 1
    fi
    echo Test succeeded!
fi
