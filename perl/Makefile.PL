use 5.006000;
use ExtUtils::MakeMaker;
use ExtUtils::PkgConfig;

my %elm_pkg_info = ExtUtils::PkgConfig->find('elementary');
my %evas_pkg_info = ExtUtils::PkgConfig->find('evas');
my %ewebview_pkg_info = ExtUtils::PkgConfig->find('ewebview');

# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile(
    NAME              => 'eWebView',
    VERSION_FROM      => 'lib/eWebView.pm', # finds $VERSION, requires EU::MM from perl >= 5.5
    CONFIGURE_REQUIRES => {'ExtUtils::PkgConfig' => 0,},
    PREREQ_PM         => {'Test'        =>  0, 
			'pEFL' => 0,}, # e.g., Module::Name => 1.1
    ABSTRACT_FROM     => 'lib/eWebView.pm', # retrieve abstract from module
    AUTHOR            => 'Maximilian',
    #LICENSE           => 'perl',
    #Value must be from legacy list of licenses here
    #https://metacpan.org/pod/Module::Build::API
    LIBS              => $elm_pkg_info{'libs'} . $evas_pkg_info{'libs'} . $ewebview_pkg_info{'libs'}, # e.g., '-lm'
    DEFINE            => '', # e.g., '-DHAVE_SOMETHING'
    INC               => '-I. ' . $elm_pkg_info{'cflags'} . $evas_pkg_info{'cflags'} . $ewebview_pkg_info{'cflags'}, # e.g., '-I. -I/usr/include/other'
	# Un-comment this if you add C files to link with later:
    # OBJECT            => '$(O_FILES)', # link all the C files too
    XSMULTI => 1,
);
