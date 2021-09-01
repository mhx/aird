EAPI=7

inherit eutils cmake-utils

DESCRIPTION="A highly configurable backlight and fan control daemon for MacBook Air"
HOMEPAGE="https://github.com/mhx/aird"
SRC_URI="https://github.com/downloads/mhx/aird/v${P}.tar.bz2"

LICENSE="MIT"
SLOT="0"
KEYWORDS="amd64"
IUSE=""

DEPEND=">=dev-libs/boost-1.46"
RDEPEND="${DEPEND}"

src_install() {
	cmake-utils_src_install
	dodoc LICENSE README.md TODO
	doinitd contrib/gentoo/init.d/aird
}
