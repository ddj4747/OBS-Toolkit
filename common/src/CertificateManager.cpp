#include <CertificateManager.h>

#include <cstdio>

#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509v3.h>

#include <ctime>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <memory>
#include <sstream>

std::mutex CertificateManager::s_mutex;

std::string CertificateManager::openSSLError() {
	const unsigned long error = ERR_get_error();
	ERR_clear_error();
	if (error == 0) {
		return "unknown OpenSSL error";
	}

	char buffer[256];
	ERR_error_string_n(error, buffer, sizeof(buffer));
	return buffer;
}

std::string CertificateManager::displayPath(const std::filesystem::path &path) {
	const auto utf8 = path.u8string();
	return {reinterpret_cast<const char *>(utf8.data()), utf8.size()};
}

bool CertificateManager::writeBioToFile(BIO *bio, const std::filesystem::path &path) {
	char *data = nullptr;
	const long size = BIO_get_mem_data(bio, &data);
	if (size < 0 || (size > 0 && !data)) {
		std::fprintf(stderr, "CertificateManager: failed to read PEM buffer (%s)", openSSLError().c_str());
		return false;
	}

	std::error_code error;
	if (path.has_parent_path()) {
		std::filesystem::create_directories(path.parent_path(), error);
	}
	if (error) {
		std::fprintf(stderr, "CertificateManager: failed to create directory for '%s' (%s)",
			displayPath(path).c_str(), error.message().c_str());
		return false;
	}

	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if (!file) {
		std::fprintf(stderr, "CertificateManager: failed to open '%s' for writing", displayPath(path).c_str());
		return false;
	}

	file.write(data, size);
	if (!file) {
		std::fprintf(stderr, "CertificateManager: failed to write '%s'", displayPath(path).c_str());
		return false;
	}

	return true;
}

CertificateManager::X509Ptr CertificateManager::readCertificate(const std::filesystem::path &path) {
	std::ifstream file(path, std::ios::binary);
	if (!file) {
		return {nullptr, X509_free};
	}

	const std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	const BioPtr bio(BIO_new_mem_buf(contents.data(), static_cast<int>(contents.size())), BIO_free);
	if (!bio) {
		return {nullptr, X509_free};
	}

	return {PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr), X509_free};
}

bool CertificateManager::setRandomSerialNumber(X509 *certificate) {
	const BignumPtr serial(BN_new(), BN_free);
	return serial && BN_rand(serial.get(), SERIAL_NUMBER_BITS, BN_RAND_TOP_ONE, BN_RAND_BOTTOM_ANY) == 1 &&
	       BN_to_ASN1_INTEGER(serial.get(), X509_get_serialNumber(certificate)) != nullptr;
}

bool CertificateManager::setSubjectAndIssuer(X509 *certificate) {
	const X509NamePtr name(X509_NAME_new(), X509_NAME_free);
	if (!name) {
		return false;
	}

	const auto addEntry = [&name](const char *field, const char *value) {
		return X509_NAME_add_entry_by_txt(name.get(), field, MBSTRING_ASC,
						  reinterpret_cast<const unsigned char *>(value), -1, -1, 0) == 1;
	};

	return addEntry("C", "PL") && addEntry("O", "OBS Toolkit") && addEntry("CN", "localhost") &&
	       X509_set_subject_name(certificate, name.get()) == 1 &&
	       X509_set_issuer_name(certificate, name.get()) == 1;
}

bool CertificateManager::addCaBasicConstraints(X509 *certificate) {
	X509V3_CTX context;
	X509V3_set_ctx_nodb(&context);
	X509V3_set_ctx(&context, certificate, certificate, nullptr, nullptr, 0);

	X509_EXTENSION *extension = X509V3_EXT_conf_nid(nullptr, &context, NID_basic_constraints, "critical,CA:TRUE");
	const bool added = extension && X509_add_ext(certificate, extension, -1) == 1;
	X509_EXTENSION_free(extension);
	return added;
}

std::string CertificateManager::toHex(const unsigned char *data, const unsigned int length) {
	std::ostringstream stream;
	stream << std::hex << std::setfill('0');
	for (unsigned int i = 0; i < length; ++i) {
		stream << std::setw(2) << static_cast<int>(data[i]);
	}
	return stream.str();
}

bool CertificateManager::generateCertificate(const std::filesystem::path &privateKeyPath,
					     const std::filesystem::path &certificatePath) {
	std::lock_guard lock(s_mutex);

	const EvpPkeyPtr privateKey(EVP_EC_gen("P-256"), EVP_PKEY_free);
	if (!privateKey) {
		std::fprintf(stderr, "CertificateManager: failed to generate EC key (%s)", openSSLError().c_str());
		return false;
	}

	const X509Ptr certificate(X509_new(), X509_free);
	if (!certificate || X509_set_version(certificate.get(), X509_VERSION_3) != 1 ||
	    !setRandomSerialNumber(certificate.get()) || !X509_gmtime_adj(X509_getm_notBefore(certificate.get()), 0) ||
	    !X509_gmtime_adj(X509_getm_notAfter(certificate.get()), CERTIFICATE_LIFETIME_SECONDS) ||
	    X509_set_pubkey(certificate.get(), privateKey.get()) != 1 || !setSubjectAndIssuer(certificate.get()) ||
	    !addCaBasicConstraints(certificate.get())) {
		std::fprintf(stderr, "CertificateManager: failed to build certificate (%s)", openSSLError().c_str());
		return false;
	}

	if (X509_sign(certificate.get(), privateKey.get(), EVP_sha256()) <= 0) {
		std::fprintf(stderr, "CertificateManager: failed to sign certificate (%s)", openSSLError().c_str());
		return false;
	}

	const BioPtr keyBio(BIO_new(BIO_s_mem()), BIO_free);
	const BioPtr certificateBio(BIO_new(BIO_s_mem()), BIO_free);
	if (!keyBio || !certificateBio ||
	    PEM_write_bio_PrivateKey(keyBio.get(), privateKey.get(), nullptr, nullptr, 0, nullptr, nullptr) != 1 ||
	    PEM_write_bio_X509(certificateBio.get(), certificate.get()) != 1) {
		std::fprintf(stderr, "CertificateManager: failed to encode PEM (%s)", openSSLError().c_str());
		return false;
	}

	return writeBioToFile(keyBio.get(), privateKeyPath) && writeBioToFile(certificateBio.get(), certificatePath);
}

bool CertificateManager::isCertificateValid(const std::filesystem::path &certificatePath) {
	std::lock_guard lock(s_mutex);

	const X509Ptr certificate = readCertificate(certificatePath);
	if (!certificate) {
		return false;
	}

	const time_t now = time(nullptr);
	const int notBefore = ASN1_TIME_cmp_time_t(X509_get0_notBefore(certificate.get()), now);
	const int notAfter = ASN1_TIME_cmp_time_t(X509_get0_notAfter(certificate.get()),
						  now + CERTIFICATE_MINIMAL_TIME_LEFT_SECONDS);

	return (notBefore == -1 || notBefore == 0) && notAfter == 1;
}

bool CertificateManager::isPrivateKeyValid(const std::filesystem::path &privateKeyPath) {
	std::lock_guard lock(s_mutex);

	std::ifstream file(privateKeyPath, std::ios::binary);
	if (!file) {
		return false;
	}

	const std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	const BioPtr bio(BIO_new_mem_buf(contents.data(), static_cast<int>(contents.size())), BIO_free);
	if (!bio) {
		return false;
	}

	const EvpPkeyPtr key(PEM_read_bio_PrivateKey(bio.get(), nullptr, nullptr, nullptr), EVP_PKEY_free);
	return key != nullptr;
}

std::string CertificateManager::getCertificateFingerprint(const std::filesystem::path &certificatePath) {
	std::lock_guard lock(s_mutex);

	const X509Ptr certificate = readCertificate(certificatePath);
	if (!certificate) {
		return {};
	}

	unsigned char digest[EVP_MAX_MD_SIZE]{};
	unsigned int digestLength = 0;
	if (X509_digest(certificate.get(), EVP_sha256(), digest, &digestLength) != 1 || digestLength == 0) {
		return {};
	}

	return toHex(digest, digestLength);
}
