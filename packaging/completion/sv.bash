# bash completion for Shadow Vault CLI (svcli)
_svcli_completions()
{
    local cur prev opts
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"
    opts="create unlock lock put get list remove rename mkdir verify repair change-password add-keyfile remove-keyfile backup-header restore-header create-hidden mount dismount benchmark portable-status --help --version --json"

    if [[ ${cur} == -* ]]; then
        COMPREPLY=( $(compgen -W "--help --version --json --password-file" -- ${cur}) )
        return 0
    fi

    if [[ ${COMP_CWORD} -eq 1 ]]; then
        COMPREPLY=( $(compgen -W "${opts}" -- ${cur}) )
        return 0
    fi
}

complete -F _svcli_completions svcli sv