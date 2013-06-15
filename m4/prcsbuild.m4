AC_DEFUN([PRCS_DEFAULT_ENV_VAR],
[AC_CACHE_CHECK([for default environment variable $1], [ac_cv_prcs_var_$1],
[if test "x$$1" = x -o "$ENVVARS" = no; then
  ac_cv_prcs_var_$1=NULL
else
  ac_cv_prcs_var_$1=\""$$1"\"
fi
])
DEFAULT_ENV_$1=$ac_cv_prcs_var_$1
AC_SUBST([DEFAULT_ENV_$1])
])
