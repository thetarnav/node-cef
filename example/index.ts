import * as cef     from '../src/index.js'
import * as process from 'node:process'
import * as path    from 'node:path'

console.log('Current directory:', process.cwd())

async function main() {
    cef.init()

    const htmlPath = path.resolve(process.cwd(), 'example', 'index.html')
    const url = 'file://' + htmlPath
    console.log('Loading URL:', url)

    const token = cef.createWindow(url, msg => {
        console.log('Message from browser:', msg)
        cef.send(token, 'echo:' + msg)
    })

    console.log('Window created with token:', token)
    console.log('Press Ctrl+C to quit')

    process.on('sigint', () => {
        console.log('Shutting down...')
        cef.close(token)
        cef.shutdown()
        process.exit(0)
    })
}

main().catch(console.error)
