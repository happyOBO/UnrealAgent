/**
 * 지정된 ID 접두사의 항목이 보이도록 스크롤합니다.
 */
export function scrollToItem(prefix, index)
{
    const el = document.getElementById(prefix + '-' + index);
    if (el)
        el.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
}