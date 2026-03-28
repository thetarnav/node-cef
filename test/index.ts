import * as cef from '../src/index.js'
import * as url from 'node:url'

interface Test_Result {
    name: string
    status: 'passed' | 'failed'
}

interface Test_Report {
    type: 'test-complete'
    passed: number
    failed: number
    tests: Test_Result[]
}

const TEST_HTML_PATH = url.fileURLToPath(new URL('index.html', import.meta.url))

async function run_tests(): Promise<void> {
    console.log('Initializing CEF...')
    cef.init()

    console.log('Creating browser window...')
    let token = cef.createWindow('file://' + TEST_HTML_PATH, msg => {
        try {
            let report: Test_Report = JSON.parse(msg)
            if (report.type === 'test-complete') {
                console.log('\n=== Test Results ===')
                console.log(`Passed: ${report.passed}`)
                console.log(`Failed: ${report.failed}`)
                console.log('')
                for (let test of report.tests) {
                    let icon = test.status === 'passed' ? '✓' : '✗'
                    console.log(`  ${icon} ${test.name}`)
                }
                console.log('')
                if (report.failed > 0) {
                    console.error('TESTS FAILED')
                    cef.shutdown()
                    process.exit(1)
                } else {
                    console.log('ALL TESTS PASSED')
                    cef.shutdown()
                    process.exit(0)
                }
            }
        } catch (e) {
            console.error('Error parsing message:', e)
        }
    })

    console.log(`Window created with token: ${token}`)
    console.log('Running tests in browser...')

    let tests = [
        {name: 'cef.send works', fn: (g: any) => g.cef && typeof g.cef.send === 'function'},
        {name: 'cef.onmessage property exists', fn: (g: any) => g.cef && 'onmessage' in g.cef},
        {name: 'simple assertion (true)', fn: () => true},
        {name: 'math works (1+1=2)', fn: () => 1 + 1 === 2},
    ]

    let browser_tests = tests.map(t => ({
        name: t.name,
        fn: `(function() {return (${t.fn.toString()})(window)})()`
    }))

    cef.send(token, JSON.stringify({type: 'run-tests', tests: browser_tests}))

    setTimeout(() => {
        console.error('Test timeout - no response received')
        cef.shutdown()
        process.exit(1)
    }, 30000)
}

try {
    run_tests()
} catch (error) {
    console.error('Test failed:', error);
    process.exit(1);
}
