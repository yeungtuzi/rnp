# The time utility must be installed. The shell built-in lacks the requisite features.
# This must be run from the base of the rnp repository.

TMPBASE=/tmp;
TMPDIR="$TMPBASE"/$$.tmp;
SFILEDIR=./test_data;
mkdir "$TMPDIR";
mkdir "$TMPDIR"/gpg;
# mkdir "$TMPDIR"/gpg/.rnp;
mkdir "$TMPDIR"/files;
GPGHDIR="$TMPDIR"/gpg/.rnp;
RNPHDIR="$TMPDIR"/gpg;
TSTN=Test
TSTEML=rnptest@ribose.com
cp -pRP "$SFILEDIR"/.rnp "$RNPHDIR"/;
cat "$SFILEDIR"/BAV.txt.bz2 > "$TMPDIR"/files/bin.bin;
# bzcat "$SFILEDIR"/BAV.txt.bz2 > "$TMPDIR"/files/long.txt
bzcat "$SFILEDIR"/BAV.txt.bz2 | tail -n +42 | head -n 140 > "$TMPDIR"/files/short.txt
RNP="./src/rnp/rnp";
GPG="gpg";
# ERRDEST="$TMPDIR"/err.txt;
ERRDEST=/dev/stderr;

echo '# Instructions for GPG to create a test key follow.
%no-protection
%transient-key
Key-Type: RSA-2048
Key-Length: 2048
# Passphrase: 
Name-Real: Test
Name-Email: rnptest@ribose.com
Expire-Date: 0
' > "$TMPDIR"/gpg/kgi;
# We don't currently run the key generation live since most distributions ship an old version of gpg without the transient key option, which requires an impractical amount of entropy to generate a key on a headless system and would hang the test.
# gpg --gen-key --homedir="$TMPDIR"/gpg/.rnp --batch "$TMPDIR"/gpg/kgi
# But we also can't use rnp because its headless keys are not compatible with gpg and it lacks robust options for automated key creation.

TC=0;

# Encryption integrity.
# Initial rnp encryption.
"$RNP" --encrypt --homedir="$TMPDIR"/gpg --userid="$TSTEML" --output="$TMPDIR"/files/bin_rnp_unrnp.gpg "$TMPDIR"/files/bin.bin 2>> "$ERRDEST";
"$RNP" --encrypt --homedir="$TMPDIR"/gpg --userid="$TSTEML" --output="$TMPDIR"/files/bin_rnp_ungpg.gpg "$TMPDIR"/files/bin.bin 2>> "$ERRDEST";
# Initial gpg encryption.
"$GPG" --encrypt --homedir="$TMPDIR"/gpg/.rnp -r "$TSTEML" --output="$TMPDIR"/files/bin_gpg_unrnp.gpg "$TMPDIR"/files/bin.bin 2>> "$ERRDEST";
"$GPG" --encrypt --homedir="$TMPDIR"/gpg/.rnp -r "$TSTEML" --output="$TMPDIR"/files/bin_gpg_ungpg.gpg "$TMPDIR"/files/bin.bin 2>> "$ERRDEST";
# rnp decryption of rnp encryption.
"$RNP" --decrypt --homedir="$TMPDIR"/gpg --output="$TMPDIR"/files/bin_rnp_unrnp "$TMPDIR"/files/bin_rnp_unrnp.gpg 2>> "$ERRDEST";
# gpg decryption of rnp encryption.
"$GPG" --decrypt --homedir="$TMPDIR"/gpg/.rnp --output="$TMPDIR"/files/bin_rnp_ungpg "$TMPDIR"/files/bin_rnp_ungpg.gpg 2>> "$ERRDEST";
# rnp decryption of gpg encryption
"$RNP" --decrypt --homedir="$TMPDIR"/gpg --output="$TMPDIR"/files/bin_gpg_unrnp "$TMPDIR"/files/bin_gpg_unrnp.gpg 2>> "$ERRDEST";
# gpg decryption of gpg encryption.
"$GPG" --decrypt --homedir="$TMPDIR"/gpg/.rnp --output="$TMPDIR"/files/bin_gpg_ungpg "$TMPDIR"/files/bin_gpg_ungpg.gpg 2>> "$ERRDEST";

TC=`expr $TC + 1`;
diff "$TMPDIR"/files/bin.bin "$TMPDIR"/files/bin_rnp_unrnp > /dev/null && echo "ok $TC rnp + unrnp encrypt/decrypt output validation" || echo "not ok $TC rnp + unrnp encrypt/decrypt output validation";
TC=`expr $TC + 1`;
diff "$TMPDIR"/files/bin.bin "$TMPDIR"/files/bin_rnp_ungpg > /dev/null && echo "ok $TC rnp + ungpg encrypt/decrypt output validation" || echo "not ok $TC rnp + ungpg encrypt/decrypt output validation";
TC=`expr $TC + 1`;
diff "$TMPDIR"/files/bin.bin "$TMPDIR"/files/bin_gpg_unrnp > /dev/null && echo "ok $TC gpg + unrnp encrypt/decrypt output validation" || echo "not ok $TC gpg + unrnp encrypt/decrypt output validation";
TC=`expr $TC + 1`;
diff "$TMPDIR"/files/bin.bin "$TMPDIR"/files/bin_gpg_ungpg > /dev/null && echo "ok $TC gpg + ungpg encrypt/decrypt output validation" || echo "not ok $TC gpg + ungpg encrypt/decrypt output validation";

# Signing integrity.
TC=`expr $TC + 1`;
cp -pRP "$TMPDIR"/files/bin.bin "$TMPDIR"/files/bin_s_rnp.bin &&
"$RNP" --sign --detach --homedir="$TMPDIR"/gpg --userid="$TSTEML" "$TMPDIR"/files/bin_s_rnp.bin 2>> "$ERRDEST" &&
echo "ok $TC signing with rnp" || echo "not ok $TC signing with rnp";
TC=`expr $TC + 1`;
cp -pRP "$TMPDIR"/files/bin.bin "$TMPDIR"/files/bin_s_gpg.bin &&
"$GPG" --detach-sign --homedir="$TMPDIR"/gpg/.rnp --local-user="$TSTEML" "$TMPDIR"/files/bin_s_gpg.bin 2>> "$ERRDEST" &&
echo "ok $TC signing with gpg" || echo "not ok $TC signing with gpg";

TC=`expr $TC + 1`;
"$RNP" --verify --homedir="$TMPDIR"/gpg "$TMPDIR"/files/bin_s_rnp.bin.sig 2>> "$ERRDEST" &&
echo "ok $TC verifying rnp sign with rnp" || echo "not ok $TC verifying rnp sign with rnp";

TC=`expr $TC + 1`;
"$GPG" --homedir="$TMPDIR"/gpg/.rnp --verify "$TMPDIR"/files/bin_s_rnp.bin.sig "$TMPDIR"/files/bin_s_rnp.bin 2>> "$ERRDEST" &&
echo "ok $TC verifying rnp sign with gpg" || echo "not ok $TC verifying rnp sign with gpg";

TC=`expr $TC + 1`;
"$RNP" --verify --homedir="$TMPDIR"/gpg "$TMPDIR"/files/bin_s_gpg.bin.sig 2>> "$ERRDEST" &&
echo "ok $TC verifying gpg sign with rnp" || echo "not ok $TC verifying gpg sign with rnp";

TC=`expr $TC + 1`;
"$GPG" --homedir="$TMPDIR"/gpg/.rnp --verify "$TMPDIR"/files/bin_s_gpg.bin.sig "$TMPDIR"/files/bin_s_gpg.bin 2>> "$ERRDEST" &&
echo "ok $TC verifying gpg sign with gpg" || echo "not ok $TC verifying gpg sign with gpg";

echo '# Timing large file signatures in RNP.'
echo -n '# ';
`which time` --output /dev/stdout bash -c "(cp -pRP \"""$TMPDIR""\"/files/bin.bin \"""$TMPDIR""\"/files/bin_ss_rnp.bin; xc=0; while [ \$xc -lt 64 ]; do xc=\`expr \$xc + 1\`; rm -f \"""$TMPDIR""\"/files/bin_ss_rnp.bin.sig; \"""$RNP""\" --sign --detach --homedir=\"""$TMPDIR""\"/gpg --userid=\"""$TSTEML""\" \"""$TMPDIR""\"/files/bin_ss_rnp.bin 2>> \"""$ERRDEST""\"; done; )";
echo '# Timing large file signatures in GPG.'
echo -n '# ';
`which time` --output /dev/stdout bash -c "(cp -pRP \"""$TMPDIR""\"/files/bin.bin \"""$TMPDIR""\"/files/bin_ss_gpg.bin; xc=0; while [ \$xc -lt 64 ]; do xc=\`expr \$xc + 1\`; rm -f \"""$TMPDIR""\"/files/bin_ss_gpg.bin.sig; gpg --homedir=\"""$TMPDIR""\"/gpg/.rnp --detach-sign --local-user=\"""$TSTEML""\" \"""$TMPDIR""\"/files/bin_ss_gpg.bin 2>> \"""$ERRDEST""\"; done; )";
echo '# Timing large file verifications in RNP.'
echo -n '# ';
`which time` --output /dev/stdout bash -c "(xc=0; while [ \$xc -lt 64 ]; do xc=\`expr \$xc + 1\`; \"""$RNP""\" --verify --homedir=\"""$TMPDIR""\"/gpg --userid=\"""$TSTEML""\" \"""$TMPDIR""\"/files/bin_ss_rnp.bin.sig 2>> \"""$ERRDEST""\"; done; )";
echo '# Timing large file verifications in GPG.'
echo -n '# ';
`which time` --output /dev/stdout bash -c "(xc=0; while [ \$xc -lt 64 ]; do xc=\`expr \$xc + 1\`; gpg --homedir=\"""$TMPDIR""\"/gpg/.rnp --verify \"""$TMPDIR""\"/files/bin_ss_gpg.bin.sig 2>> \"""$ERRDEST""\"; done; )";

echo '# Timing small file signatures in RNP.'
echo -n '# ';
`which time` --output /dev/stdout bash -c "(cp -pRP \"""$TMPDIR""\"/files/short.txt \"""$TMPDIR""\"/files/short_ss_rnp.txt; xc=0; while [ \$xc -lt 64 ]; do xc=\`expr \$xc + 1\`; rm -f \"""$TMPDIR""\"/files/short_ss_rnp.txt.sig; \"""$RNP""\" --sign --detach --homedir=\"""$TMPDIR""\"/gpg --userid=\"""$TSTEML""\" \"""$TMPDIR""\"/files/short_ss_rnp.txt 2>> \"""$ERRDEST""\"; done; )";
echo '# Timing small file signatures in GPG.'
echo -n '# ';
`which time` --output /dev/stdout bash -c "(cp -pRP \"""$TMPDIR""\"/files/bin.bin \"""$TMPDIR""\"/files/short_ss_gpg.txt; xc=0; while [ \$xc -lt 64 ]; do xc=\`expr \$xc + 1\`; rm -f \"""$TMPDIR""\"/files/short_ss_gpg.txt.sig; gpg --homedir=\"""$TMPDIR""\"/gpg/.rnp --detach-sign --local-user=\"""$TSTEML""\" \"""$TMPDIR""\"/files/short_ss_gpg.txt 2>> \"""$ERRDEST""\"; done; )";
echo '# Timing small file verifications in RNP.'
echo -n '# ';
`which time` --output /dev/stdout bash -c "(xc=0; while [ \$xc -lt 64 ]; do xc=\`expr \$xc + 1\`; \"""$RNP""\" --verify --homedir=\"""$TMPDIR""\"/gpg --userid=\"""$TSTEML""\" \"""$TMPDIR""\"/files/short_ss_rnp.txt.sig 2>> \"""$ERRDEST""\"; done; )";
echo '# Timing small file verifications in GPG.'
echo -n '# ';
`which time` --output /dev/stdout bash -c "(xc=0; while [ \$xc -lt 64 ]; do xc=\`expr \$xc + 1\`; gpg --homedir=\"""$TMPDIR""\"/gpg/.rnp --verify \"""$TMPDIR""\"/files/short_ss_gpg.txt.sig 2>> \"""$ERRDEST""\"; done; )";

echo '# Timing large file encryptions in RNP.'
echo -n '# ';
`which time` --output /dev/stdout bash -c "(cp -pRP \"""$TMPDIR""\"/files/bin.bin \"""$TMPDIR""\"/files/bin_es.bin; xc=0; while [ \$xc -lt 64 ]; do xc=\`expr \$xc + 1\`; rm -f \"""$TMPDIR""\"/files/bin_es_rnp.gpg; \"""$RNP""\" --encrypt --homedir=\"""$TMPDIR""\"/gpg --userid=\"""$TSTEML""\" --output=\"""$TMPDIR""\"/files/bin_es_rnp.gpg \"""$TMPDIR""\"/files/bin_es.bin 2>> \"""$ERRDEST""\"; done; )";
echo '# Timing large file encryptions in GPG.'
echo -n '# ';
`which time` --output /dev/stdout bash -c "(cp -pRP \"""$TMPDIR""\"/files/bin.bin \"""$TMPDIR""\"/files/bin_es.bin; xc=0; while [ \$xc -lt 64 ]; do xc=\`expr \$xc + 1\`; rm -f \"""$TMPDIR""\"/files/bin_es_gpg.gpg; \"""$GPG""\" --homedir=\"""$TMPDIR""\"/gpg/.rnp --encrypt -r \"""$TSTEML""\" --local-user=\"""$TSTEML""\" --output=\"""$TMPDIR""\"/files/bin_es_gpg.gpg \"""$TMPDIR""\"/files/bin_es.bin 2>> \"""$ERRDEST""\"; done; )";
echo '# Timing large file decryptions in RNP.'
echo -n '# ';
`which time` --output /dev/stdout bash -c "(xc=0; while [ \$xc -lt 64 ]; do xc=\`expr \$xc + 1\`; rm -f \"""$TMPDIR""\"/files/bin_es_rnp; \"""$RNP""\" --decrypt --homedir=\"""$TMPDIR""\"/gpg --output=\"""$TMPDIR""\"/files/bin_es_rnp \"""$TMPDIR""\"/files/bin_es_rnp.gpg 2>> \"""$ERRDEST""\"; done; )";
echo '# Timing large file decryptions in GPG.'
echo -n '# ';
`which time` --output /dev/stdout bash -c "(xc=0; while [ \$xc -lt 64 ]; do xc=\`expr \$xc + 1\`; rm -f \"""$TMPDIR""\"/files/bin_es_gpg; \"""$GPG""\" --homedir=\"""$TMPDIR""\"/gpg/.rnp --decrypt --output=\"""$TMPDIR""\"/files/bin_es_gpg \"""$TMPDIR""\"/files/bin_es_gpg.gpg 2>> \"""$ERRDEST""\"; done; )";

echo '# Timing small file encryptions in RNP.'
echo -n '# ';
`which time` --output /dev/stdout bash -c "(cp -pRP \"""$TMPDIR""\"/files/short.txt \"""$TMPDIR""\"/files/short_es.txt; xc=0; while [ \$xc -lt 64 ]; do xc=\`expr \$xc + 1\`; rm -f \"""$TMPDIR""\"/files/short_es_rnp.gpg; \"""$RNP""\" --encrypt --homedir=\"""$TMPDIR""\"/gpg --userid=\"""$TSTEML""\" --output=\"""$TMPDIR""\"/files/short_es_rnp.gpg \"""$TMPDIR""\"/files/short_es.txt 2>> \"""$ERRDEST""\"; done; )";
echo '# Timing small file encryptions in GPG.'
echo -n '# ';
`which time` --output /dev/stdout bash -c "(cp -pRP \"""$TMPDIR""\"/files/bin.txt \"""$TMPDIR""\"/files/short_es.txt; xc=0; while [ \$xc -lt 64 ]; do xc=\`expr \$xc + 1\`; rm -f \"""$TMPDIR""\"/files/short_es_gpg.gpg; \"""$GPG""\" --homedir=\"""$TMPDIR""\"/gpg/.rnp --encrypt -r \"""$TSTEML""\" --local-user=\"""$TSTEML""\" --output=\"""$TMPDIR""\"/files/short_es_gpg.gpg \"""$TMPDIR""\"/files/short_es.txt 2>> \"""$ERRDEST""\"; done; )";
echo '# Timing small file decryptions in RNP.'
echo -n '# ';
`which time` --output /dev/stdout bash -c "(xc=0; while [ \$xc -lt 64 ]; do xc=\`expr \$xc + 1\`; rm -f \"""$TMPDIR""\"/files/short_es_rnp \"""$RNP""\" --decrypt --homedir=\"""$TMPDIR""\"/gpg --output=\"""$TMPDIR""\"/files/short_es_rnp \"""$TMPDIR""\"/files/short_es_rnp.gpg 2>> \"""$ERRDEST""\"; done; )";
echo '# Timing small file decryptions in GPG.'
echo -n '# ';
`which time` --output /dev/stdout bash -c "(xc=0; while [ \$xc -lt 64 ]; do xc=\`expr \$xc + 1\`; rm -f \"""$TMPDIR""\"/files/short_es_gpg; \"""$GPG""\" --homedir=\"""$TMPDIR""\"/gpg/.rnp --decrypt --output=\"""$TMPDIR""\"/files/short_es_gpg \"""$TMPDIR""\"/files/short_es_gpg.gpg 2>> \"""$ERRDEST""\"; done; )";

