Name:       ngfd

Summary:    Non-graphic feedback service for sounds and other events
Version:    1.4.1
Release:    1
License:    LGPLv2+
URL:        https://github.com/sailfishos/ngfd
Source0:    %{name}-%{version}.tar.gz
Source1:    ngfd.service
Requires:   %{name}-settings
Requires:   systemd
Requires:   systemd-user-session-targets
Requires:   gstreamer1.0-plugins-good
BuildRequires:  pkgconfig(glib-2.0) >= 2.40.0
BuildRequires:  pkgconfig(dbus-1) >= 1.8.0
BuildRequires:  pkgconfig(libpulse)
BuildRequires:  pkgconfig(gstreamer-1.0)
BuildRequires:  pkgconfig(gstreamer-controller-1.0)
BuildRequires:  pkgconfig(gio-2.0)
BuildRequires:  pkgconfig(gobject-2.0)
BuildRequires:  pkgconfig(gthread-2.0)
BuildRequires:  pkgconfig(check)
BuildRequires:  pkgconfig(mce)
BuildRequires:  pkgconfig(profile)
BuildRequires:  pkgconfig(libcanberra)
BuildRequires:  pkgconfig(ohm-ext-route)
BuildRequires:  pkgconfig(systemd)
BuildRequires:  libtool
BuildRequires:  doxygen
Obsoletes:      tone-generator <= 1.5.4
Provides:       tone-generator > 1.5.4

%description
This package contains the daemon servicing the non-graphical feedback
requests.


%package plugin-devel
Summary:    Development package for ngfd plugin creation
Requires:   %{name} = %{version}-%{release}

%description plugin-devel
This package contains header files for creating plugins to non-graphical feedback daemon.

%package plugin-fake
Summary:    Fake plugins for ngfd testing
Requires:   %{name} = %{version}-%{release}

%description plugin-fake
Fake plugins for ngfd testing.

%package settings-basic
Summary:    Example settings for ngfd
Requires:   %{name} = %{version}-%{release}
Provides:   %{name}-settings

%description settings-basic
Example settings for ngfd.

%package plugin-doc
Summary:    Documentation package for ngfd plugin creation
Requires:   %{name} = %{version}-%{release}

%description plugin-doc
This package contains documentation to header files for creating plugins to non-graphical feedback daemon.

%package tests
Summary:    Test suite for ngfd
Requires:   %{name} = %{version}-%{release}
Requires:   %{name}-plugin-fake = %{version}-%{release}

%description tests
This package contains test suite for ngfd.

%prep
%setup -q -n %{name}-%{version}


%build
%autogen --enable-debug
make %{?_smp_mflags}

%install
rm -rf %{buildroot}
%make_install
rm -f %{buildroot}/%{_libdir}/ngf/*.la

install -D -m 644 %{SOURCE1} %{buildroot}%{_userunitdir}/ngfd.service
mkdir -p %{buildroot}%{_userunitdir}/user-session.target.wants
ln -s ../ngfd.service %{buildroot}%{_userunitdir}/user-session.target.wants/
mkdir -p %{buildroot}%{_userunitdir}/actdead-session.target.wants
ln -s ../ngfd.service %{buildroot}%{_userunitdir}/actdead-session.target.wants/

%files
%defattr(-,root,root,-)
%license COPYING
%config %{_sysconfdir}/dbus-1/system.d/%{name}.conf
%{_bindir}/%{name}
%dir %{_libdir}/ngf
%{_libdir}/ngf/libngfd_dbus.so
%{_libdir}/ngf/libngfd_resource.so
%{_libdir}/ngf/libngfd_transform.so
%{_libdir}/ngf/libngfd_gst.so
%{_libdir}/ngf/libngfd_canberra.so
%{_libdir}/ngf/libngfd_mce.so
%{_libdir}/ngf/libngfd_streamrestore.so
%{_libdir}/ngf/libngfd_tonegen.so
%{_libdir}/ngf/libngfd_callstate.so
%{_libdir}/ngf/libngfd_profile.so
%{_libdir}/ngf/libngfd_ffmemless.so
%{_libdir}/ngf/libngfd_devicelock.so
%{_libdir}/ngf/libngfd_route.so
%{_libdir}/ngf/libngfd_null.so
%{_userunitdir}/ngfd.service
%{_userunitdir}/user-session.target.wants/ngfd.service
%{_userunitdir}/actdead-session.target.wants/ngfd.service

%files plugin-devel
%defattr(-,root,root,-)
%{_includedir}/ngf
%{_libdir}/pkgconfig/ngf-plugin.pc

%files plugin-fake
%defattr(-,root,root,-)
%{_libdir}/ngf/libngfd_fake.so
%{_libdir}/ngf/libngfd_test_fake.so

%files settings-basic
%defattr(-,root,root,-)
%{_datadir}/ngfd/

%files plugin-doc
%defattr(-,root,root,-)
%{_docdir}/ngfd-plugin/html

%files tests
%defattr(-,root,root,-)
/opt/tests/ngfd
