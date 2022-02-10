#!/bin/env bash

# TODO: As some point it might be nice to try and only ask for the password
#       for the root certificate once. But I have to look into that later.

# Directory to place Private Key and Cert files. (default: current working directory)
OUTPUT_DIR="$(pwd)/ca"

# The config file to be used, ignored if USE_CONFIG is 0
CONFIG_FILE="`dirname $(readlink -f $0)`/config/root.cfg"
# Name of the certificate and key to generate
CERT_NAME="root"
# Whether or not to change the file permissions to 600 (off by default)
PROTECT_CERT=0
# Whether or not to run a test on the generated key
RUN_TEST=0
# Base url to add to the certificate
BASE_URL=https://smoothstack.com

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
    echo "                         (defaults to config/root.cfg)"
    echo "  -n, --name           name of the private key and certificate."
    echo "                         (defaults to root.pem and root.crt)"
    echo "  -p, --protect        sets the permissions of the generated files to"
    echo "                         600 to protect them from being read by other"
    echo "                         users on the system."
    echo "  -t, --test           runs a test on the generated certificate to"
    echo "                         make sure it's ready for signing."
    echo "  -h, --help           displays this message and quits."
    echo "  -u, --url            base url to add to the certificate."
    echo
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        -h|--help) help; exit;;
        -o|--output) OUTPUT_DIR="$2"; shift 2;;
        -c|--config) CONFIG_FILE="$2"; shift 2;;
        -n|--name) CERT_NAME="$2"; shift 2;;
        -p|--protect) PROTECT_CERT=1; shift 1;;
        -t|--test) RUN_TEST=1; shift 1;;
        -u|--url) BASE_URL="$2"; shift 2;;

        -o=*|--output=*) OUTPUT_DIR="${1#*=}"; shift 1;;
        -c=*|--config=*) CONFIG_FILE="${1#*=}"; shift 1;;
        -n=*|--name=*) CERT_NAME="${1#*=}"; shift 1;;
        -u=*|--url=*) BASE_URL="${1#*=}"; shift 1;;

        -*) echo "Unknown option: $1" >&2; help; exit 1;;
        *) echo "Ignoring extra argument: $1"; shift 1;;
    esac
done

if [ ! -d $OUTPUT_DIR ]; then
    mkdir -p $OUTPUT_DIR
fi

OUTPUT_DIR=$(readlink -f $OUTPUT_DIR)

# NOTE: I utilize '$OUTPUT_DIR/$CERT_NAME/' a lot, maybe I should put it in a variable?
echo Generating certificate directory structure under $OUTPUT_DIR...
mkdir -pv $OUTPUT_DIR/crl
mkdir -pv $OUTPUT_DIR/$CERT_NAME{/private,/db}

echo Generating required database files under $OUTPUT_DIR/$CERT_NAME/db
pushd $OUTPUT_DIR/$CERT_NAME/db
cp /dev/null $CERT_NAME.db
cp /dev/null $CERT_NAME.db.attr
echo 01 > $CERT_NAME.crt.srl
echo 01 > $CERT_NAME.crl.srl
popd

echo Expanding variables in config file...
cp $CONFIG_FILE $OUTPUT_DIR/$CERT_NAME/openssl.cfg
CONFIG_FILE=$OUTPUT_DIR/$CERT_NAME/openssl.cfg
sed -i "s|%%DIRECTORY%%|$OUTPUT_DIR|g" $CONFIG_FILE
sed -i "s|%%NAME%%|$CERT_NAME|g" $CONFIG_FILE
sed -i "s|%%BASE_URL%%|$BASE_URL|g" $CONFIG_FILE

echo Generating a root Private Key and CA Request...
if ! openssl req -new -config $CONFIG_FILE -out "$OUTPUT_DIR/$CERT_NAME.csr" -keyout "$OUTPUT_DIR/$CERT_NAME/private/$CERT_NAME.pem"; then
    echo Failed to generate a private key or CA request!
    exit 1
fi
echo Successfully generated $OUTPUT_DIR/$CERT_NAME.csr and $OUTPUT_DIR/$CERT_NAME/private/$CERT_NAME.pem!

# TODO: Find some way to parameterize the enddate flag
echo Using generated CA Request to make a self-signed certificate...
if ! openssl ca -selfsign -config $CONFIG_FILE -in "$OUTPUT_DIR/$CERT_NAME.csr" -out "$OUTPUT_DIR/$CERT_NAME.crt" -extensions "root_ca_ext" -enddate 20301231235959Z
then
    echo Failed to generate a certificate!
    exit 1
fi
echo Successfully generated $OUTPUT_DIR/$CERT_NAME.crt!

echo Creating an empty CRL...
if ! openssl ca -gencrl -config $CONFIG_FILE -out "$OUTPUT_DIR/crl/$CERT_NAME.crl"; then
    echo Failed to create a empty CRL!
fi
echo Successfully created empty CRL!

if [ $RUN_TEST -eq 1 ]; then
    echo Running tests on $OUTPUT_DIR/$CERT_NAME.crt
    if ! openssl x509 -noout -text -in "$OUTPUT_DIR/$CERT_NAME.crt"; then
        echo Test failed!
        exit 1
    fi
    echo Test succeeded!
fi

if [ $PROTECT_CERT -eq 1 ]; then
    echo Protecting generated keys and certificate...
    chmod -v 400 "$OUTPUT_DIR/$CERT_NAME/private/$CERT_NAME.pem"
    chmod -v 444 "$OUTPUT_DIR/$CERT_NAME.crt"
fi
